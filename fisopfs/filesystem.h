#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_FILES 100
#define MAX_PATH 100
#define MAX_CONTENT 1024 
#define MAX_DIRECTORY_SIZE 1024
#define FREE 0
#define USED 1
#define ROOT "/"

typedef enum file_type {
	ARCH,
	DIR 
} file_type_t;

// Los structs tienen un tamaño fijo definido en tiempo de compilacion, ya que utilizamos constantes para definir el tamaño de los arreglos.
typedef struct File {
	int free_status;           // Indica si el file está libre o no
	file_type_t type;          // Tipo de archivo (ARCH o DIR)
	mode_t mode;               // Permisos del archivo
	uid_t user_id;             // ID del usuario propietario
	gid_t group_id;            // ID del grupo propietario
	time_t last_accessed_at;   // Fecha de último acceso
	time_t last_modified_at;   // Fecha de última modificación
	char path[MAX_PATH];       // Ruta del archivo
	char content[MAX_CONTENT]; // Contenido del archivo
	char dir_path[MAX_PATH];   // Ruta del directorio padre
} file_t;

struct Filesystem {
	struct File files[MAX_FILES];
};

bool
fs_position_is_this_file(int pos, const char *path, struct Filesystem* filesystem)
{
	return filesystem->files[pos].free_status == USED &&
	       strcmp(filesystem->files[pos].path, path) == 0;
}

int
fs_find_this_file(const char *path, struct Filesystem* filesystem)
{
	for (int i = 0; i < MAX_FILES; i++) {
		if (fs_position_is_this_file(i, path,filesystem)) {
			return i;
		}
	}
	return -1;
}

file_t
fs_init_root()
{
	file_t root = {
		.free_status = USED,
		.type = DIR,
		.mode = __S_IFDIR | 0755,
		.user_id = 1717,
		.group_id = getgid(),
		.last_accessed_at = time(NULL),
		.last_modified_at = time(NULL),
	};

	strcpy(root.path, ROOT);
	memset(root.content, 0, sizeof(root.content));
	strcpy(root.dir_path, "");

	return root;
}

int
fs_init_fs(struct Filesystem* filesystem)
{
	file_t root = fs_init_root();

	filesystem->files[0] = root;

	for (int i = 1; i < MAX_FILES; i++) {
		filesystem->files[i] = (file_t){ 0 };
	};

	return 0;
}

void
fs_serialize(struct Filesystem *filesystem, const char *storage)
{
	FILE *file = fopen(storage, "w");

	if (!file) {
		perror("Error opening filesystem state file for writing");
		return;
	}

	size_t written = fwrite(filesystem, sizeof(struct Filesystem), 1, file);
	if (written != 1) {
		perror("Error writing filesystem state to file");
	}

	fclose(file);
}

void
fs_deserialize(struct Filesystem *filesystem, const char *storage)
{
	FILE *file = fopen(storage, "r");
	if (!file) {
		fs_init_fs(filesystem);
		return;
	}

	size_t read = fread(filesystem, sizeof(struct Filesystem), 1, file);
	if (read != 1) {
		perror("Error reading filesystem state from file");
	}

	fclose(file);
}

int
fs_create_file(const char *path, mode_t mode,file_type_t type_received,struct Filesystem *filesystem){
	for (int i = 0; i < MAX_FILES; i++) {
		if (filesystem->files[i].free_status == FREE) {
			filesystem->files[i].free_status = USED;
			filesystem->files[i].type = type_received;
			filesystem->files[i].mode = mode;
			filesystem->files[i].user_id = fuse_get_context()->uid;
			filesystem->files[i].group_id = fuse_get_context()->gid;
			filesystem->files[i].last_accessed_at = time(NULL);
			filesystem->files[i].last_modified_at = time(NULL);
			strncpy(filesystem->files[i].path, path, MAX_PATH);

			const char *last_slash = strrchr(path, '/');
			if (last_slash != NULL && last_slash == path) {
				strncpy(filesystem->files[i].dir_path,ROOT,MAX_PATH);
			} else {
				size_t dir_length = last_slash - path;
				strncpy(filesystem->files[i].dir_path, path,dir_length);
				filesystem->files[i].dir_path[dir_length] = '\0';
			}

			filesystem->files[i].content[0] = '\0';
			return 0;
		}
	}
	return -ENOSPC;
}
