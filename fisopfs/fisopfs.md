# fisop-fs

## Documentación de diseño

Por cuestiones de simplicidad, decidimos crear un FileSystem que es un arreglo de Files de tamaño fijo. Esto quiere decir que tanto los archivos, como la cantidad de los mismos son fijos. Tanto archivos como directorios son tratados de la misma forma como un File siendo posible distinguirlos mediante un atributo del File en donde se guarda el tipo (type).

### Estructura:

```
typedef enum file_type {
    ARCH,
    DIR 
} file_type_t;
```
```
typedef struct File {
    int free_status;           // Indica si el file está libre (1) o no (0)
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
```
```
struct Filesystem {
    struct File files[MAX_FILES];
}
```
> **NOTA**: Los structs tienen un tamaño fijo definido en tiempo de compilacion, ya que utilizamos constantes para definir el tamaño de los arreglos

### Busqueda de archivo o directorio dado su path

Para poder encontrar un archivo/directorio dado un PATH, el `Filesystem` recorre todo el arreglo de `Files` buscando algun `File` cuyo `PATH` coincida con el buscado y este siendo usado (que no esté `FREE`).
 
### Formato de serialización del sistema

Lo primero que se hace cuando se ejecuta el ./fisopfs es un parseo de los argumentos recibidos. En caso de contener un argumento adicional para el nombre del archivo de guardado este queda guardado en la variable storage, caso contratio se guarda el filesystem en un archivo en un PATH predeterminado (`file.fisopf`).
Cuando se ejecuta serialize se hace un write en el archivo cuyo nombre esta guardado en la variable storage, en caso de no existir este se crea.
Cuando se deserializa se lee el archivo cuyo nombre esta en la variable storage, y se guarda el contenido del mismo en la variable filesistem.

## Pruebas:
```
$touch archivo && mkdir folder && ls
archivo folder

$cd folder && touch file.txt && ls
file.txt

$stat file.txt
File: file.txt
Size: 0         	Blocks: 0          IO Block: 4096   regular empty file
Device: 4eh/78d	Inode: 4           Links: 1
Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
Access: 2024-06-13 14:53:59.000000000 +0000
Modify: 2024-06-13 14:53:59.000000000 +0000
Change: 1970-01-01 00:00:00.000000000 +0000

$echo "hola mundo" > file.txt && cat file.txt
hola mundo

$stat file.txt
File: file.txt
Size: 11        	Blocks: 0          IO Block: 4096   regular file
Device: 4eh/78d	Inode: 4           Links: 1
Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
Access: 2024-06-13 14:54:17.000000000 +0000
Modify: 2024-06-13 14:54:17.000000000 +0000
Change: 1970-01-01 00:00:00.000000000 +0000

$touch a && rm file.txt && ls
a

$cd .. && rmdir folder && ls
archivo

$touch archivo && echo "probando" > archivo && echo "prueba" >> archivo && less archivo
probando
prueba
(END)
```







