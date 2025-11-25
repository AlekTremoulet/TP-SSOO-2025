#ifndef STORAGE_MAIN_
#define STORAGE_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <commons/crypto.h>

t_log *logger;
t_config *config;
t_config *configSuperblock;
t_log_level current_log_level;
char * puerto;
char * fresh_start;
char * punto_montaje;
char * dir_physical_blocks;
char * dir_files;
char * path_hash;
int tam_fs;
int tam_bloque;
int block_count;

char *path_bitmap;

typedef struct {
    char * nombre;
    char * hash;
    char * ruta_base;
    char * ruta_tag;
} t_archivo_creado;

typedef struct {
    char * Estado;
    char * Blocks; 
    char * Tamanio;
} t_bloque_fisico;



int main(int argc, char* argv[]);
void levantarConfig();
void levantarConfigSuperblock();
void* server_mh_worker(void *args);
char* crear_directorio(char* path_a_crear);
char* borrar_directorio(const char* path_a_borrar);
char *cargar_archivo(char * ruta_base ,char *ruta_al_archivo);
void inicializar_hash();
char *escribir_en_hash(char *nombre_bloque);
void inicializar_bloque_fisico(int numero_bloque);
void inicializar_bloques_logicos();
char * crear_archivo_en_FS(char *nombre_archivo, char *tag_archivo);

#endif