#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "filesystem.h"
#include <fcntl.h>

// --------------- COMO COMPILAR Y EJECUTAR---------------- //
/*

 +++++ ABRIR UNA TERMINAL EN ESTE DIRECTORIO +++++
sudo make docker-build
sudo make docker-run
make
mkdir prueba
./fisopfs -f ./prueba       o     ./fisopfs -f prueba storage

 ++++++++++ EN OTRA TERMINAL +++++++++
sudo make docker-attach
cd prueba

*/

// ------------------------------------------------------ //

struct Filesystem filesystem = {};

int size_of_files = MAX_FILES * sizeof(struct File);

char *storage;


static int
fisopfs_mkdir(const char *path, mode_t mode)
{
	printf("[debug] fisopfs_mkdir - path: %s\n", path);

	// Verificamos si el path ya existe
	if (fs_find_this_file(path, &filesystem) != -1) {
		return -EEXIST;
	}

	return fs_create_file(path, mode, DIR, &filesystem);
}

static void *
fisopfs_init(struct fuse_conn_info *conn)
{
	printf("[debug] fisopfs_init - conn: %p\n", conn);

	fs_deserialize(&filesystem, storage);

	return NULL;
}

static int
fisopfs_write(const char *path,
              const char *data,
              size_t size_data,
              off_t offset,
              struct fuse_file_info *fuse_info)
{
	printf("[debug] fisopfs_write - path: %s\n", path);

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		if (filesystem.files[file_pos].type != ARCH) {
			return -EISDIR;  // Es un directorio, no se puede escribir como un archivo
		}

		// Verificar que el offset y size_data son válidos
		if (offset + size_data > MAX_CONTENT) {
			return -ENOSPC;  // No hay espacio suficiente
		}

		// Escribir los datos en el archivo
		memcpy(filesystem.files[file_pos].content + offset, data, size_data);
		filesystem.files[file_pos].content[offset + size_data] = '\0';
		filesystem.files[file_pos].last_modified_at = time(NULL);
		return size_data;
	}
}


static int
fisopfs_rmdir(const char *path)
{
	printf("[debug] fisopfs_rmdir - path: %s\n", path);

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		if (filesystem.files[file_pos].type != DIR) {
			return -ENOTDIR;  // No es un directorio
		}
		// Verificar que el directorio está vacío (no tiene archivos ni subdirectorios
	}
	for (int j = 0; j < MAX_FILES; j++) {
		if (filesystem.files[j].free_status == USED &&
		    strcmp(filesystem.files[j].dir_path, path) == 0) {
			// Con indicar que el archivo está libre es suficiente para eliminarlo
			filesystem.files[j].free_status = FREE;
		}
	}
	filesystem.files[file_pos].free_status = FREE;
	return 0;
}

static int
fisopfs_unlink(const char *path)
{
	printf("[debug] fisopfs_unlink - path: %s\n", path);

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		if (filesystem.files[file_pos].type != ARCH) {
			return -EISDIR;  // Es un directorio, no se puede eliminar con unlink
		}
		filesystem.files[file_pos].free_status = FREE;
		return 0;
	}
}

void
fisopfs_destroy(void *private_data)
{
	printf("[debug] fisopfs_destroy - private_data: %p\n", private_data);

	fs_serialize(&filesystem, storage);
	free(storage);
}

static int
fisopfs_flush(const char *path, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_flush - path: %s\n", path);

	fs_serialize(&filesystem, storage);

	return 0;
}

static int
fisopfs_truncate(const char *path, off_t size)
{
	printf("[debug] fisopfs_truncate - path: %s, size: %ld\n", path, size);

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		if (filesystem.files[file_pos].type == ARCH) {
			if (size < strlen(filesystem.files[file_pos].content)) {
				filesystem.files[file_pos].content[size] = '\0';
			}
			return 0;
		}
	}
	return -EISDIR;
}

static int
fisopfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_create - path: %s\n", path);

	// Verificamos que el path es válido
	if (path == NULL || strlen(path) == 0 || path[0] != '/') {
		return -EINVAL;
	}

	// Verificamos si el archivo con este path ya existe

	if (fs_find_this_file(path, &filesystem) != -1) {
		return -EEXIST;
	}

	// Buscar un lugar libre en el sistema de archivos
	return fs_create_file(path, mode, ARCH, &filesystem);
}

static int
fisopfs_utimens(const char *path, const struct timespec tv[2])
{
	printf("[debug] fisopfs_utimens - path: %s\n", path);

	// Buscamos el archivo en el sistema de archivos
	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		filesystem.files[file_pos].last_accessed_at = tv[0].tv_sec;
		filesystem.files[file_pos].last_modified_at = tv[1].tv_sec;
		return 0;
	}
}

static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr - path: %s\n", path);

	memset(st, 0, sizeof(struct stat));

	// Verificar si el path es válido
	if (path == NULL || strlen(path) == 0 || path[0] != '/') {
		return -EINVAL;
	}

	// Si el path es la raíz
	if (strcmp(path, ROOT) == 0) {
		st->st_mode =
		        __S_IFDIR | 0755;  // Directorio con permisos rwxr-xr-x
		st->st_nlink =
		        2;  // Los directorios tienen al menos 2 enlaces (a sí mismos y a su padre)
		return 0;
	}

	// Buscar el archivo en el sistema de archivos

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		// Archivo encontrado, llenar la estructura stat
		st->st_mode = filesystem.files[file_pos].mode;
		st->st_uid = filesystem.files[file_pos].user_id;
		st->st_gid = filesystem.files[file_pos].group_id;
		st->st_atime = filesystem.files[file_pos].last_accessed_at;
		st->st_mtime = filesystem.files[file_pos].last_modified_at;

		if (filesystem.files[file_pos].type == DIR) {
			st->st_mode |= __S_IFDIR;
			st->st_nlink = 2;
		} else {
			st->st_mode |= __S_IFREG;
			st->st_nlink = 1;  // Un archivo regular tiene un enlace
			st->st_size = strlen(filesystem.files[file_pos].content);
		}
		return 0;
	}
}

int
alocate_store_path(int *argc, char *argv[])
{
	storage = malloc(strlen("file.fisopfs") + 1);
	if (storage == NULL) {
		perror("Failed to allocate memory for storage\n");
		return -1;
	}
	strcpy(storage, "file.fisopfs");
	if (*argc > 2) {
		char *second_last = argv[*argc - 2];
		char *last = argv[*argc - 1];
		if (second_last[0] != '-' && second_last[0] != '.') {
			char *resultado =
			        malloc(strlen(last) + strlen(".fisopfs") + 1);
			if (resultado == NULL) {
				perror("Failed to allocate memory for "
				       "resultado\n");
				free(storage);
				return -1;
			}
			strcpy(resultado, last);
			strcat(resultado, ".fisopfs");
			free(storage);
			storage = (char *) malloc(strlen(resultado) + 1);
			if (storage != NULL) {
				strcpy(storage, resultado);
				storage[strlen(resultado)] = '\0';
				last[0] = '\0';
				(*argc)--;
			} else {
				free(resultado);
				perror("Failed to allocate memory for "
				       "storage\n");
				return -1;
			}
			free(resultado);
		}
	}
	return 0;
}

static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_readdir - path: %s\n", path);

	// Verificar que el path es válido
	if (path == NULL || strlen(path) == 0 || path[0] != '/') {
		return -EINVAL;
	}

	for (int i = 0; i < MAX_FILES; i++) {
		if (filesystem.files[i].free_status == USED) {
			char *last_separator =
			        strrchr(filesystem.files[i].path, '/');
			if (last_separator != NULL) {
				if (strcmp(last_separator + 1, path + 1) == 0 &&
				    filesystem.files[i].type == DIR) {
					filler(buffer, ".", NULL, 0);
					filler(buffer, "..", NULL, 0);

					// Listar los archivos y directorios contenidos en el directorio especificado
					for (int j = 0; j < MAX_FILES; j++) {
						if (filesystem.files[j].free_status ==
						            USED &&
						    strcmp(filesystem.files[j].dir_path,
						           path) == 0) {
							if (strcmp(path, ROOT) ==
							    0) {
								filler(buffer,
								       filesystem.files[j]
								                       .path +
								               strlen(path),
								       NULL,
								       0);
							} else {
								filler(buffer,
								       filesystem.files[j]
								                       .path +
								               strlen(path) +
								               1,
								       NULL,
								       0);
							}
						}
					}
					return 0;
				}
			}
		}
	}
	return -ENOENT;
}

static int
fisopfs_read(const char *path,
             char *buffer,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_read - path: %s, offset: %lu, size: %lu\n",
	       path,
	       offset,
	       size);

	// Buscar el archivo en el sistema de archivos

	int file_pos = fs_find_this_file(path, &filesystem);
	if (file_pos == -1) {
		return -ENOENT;
	} else {
		// Verificar que es un archivo regular
		if (filesystem.files[file_pos].type != ARCH) {
			return -EISDIR;  // Es un directorio, no se puede leer como un archivo
		}

		// Verificar que el offset y size son válidos
		if (offset >= strlen(filesystem.files[file_pos].content)) {
			return 0;  // Offset está más allá del final del archivo
		}

		if (offset + size > strlen(filesystem.files[file_pos].content)) {
			size = strlen(filesystem.files[file_pos].content) - offset;
		}

		memcpy(buffer, filesystem.files[file_pos].content + offset, size);
		filesystem.files[file_pos].last_accessed_at = time(NULL);
		return size;
	}
}


static struct fuse_operations operations = { .getattr = fisopfs_getattr,
	                                     .readdir = fisopfs_readdir,
	                                     .read = fisopfs_read,
	                                     .mkdir = fisopfs_mkdir,
	                                     .init = fisopfs_init,
	                                     .write = fisopfs_write,
	                                     .create = fisopfs_create,
	                                     .rmdir = fisopfs_rmdir,
	                                     .unlink = fisopfs_unlink,
	                                     .flush = fisopfs_flush,
	                                     .destroy = fisopfs_destroy,
	                                     .truncate = fisopfs_truncate,
	                                     .utimens = fisopfs_utimens };


int
main(int argc, char *argv[])
{
	int returned_value = alocate_store_path(&argc, argv) != 0;
	if (returned_value != 0) {
		return -returned_value;
	} else {
		return fuse_main(argc, argv, &operations, NULL);
	}
}
