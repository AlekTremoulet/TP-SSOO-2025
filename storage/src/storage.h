#ifndef STORAGE_MAIN_
#define STORAGE_MAIN_

#include <operaciones.h>

extern FILE* bitmap_file;

t_log *logger;
t_config *config;
t_config *configSuperblock;
t_log_level current_log_level;
char * puerto;
char * fresh_start;
char * punto_montaje;
int retardo_operacion;
int retardo_bloque;
char * dir_physical_blocks;
char * dir_files;
char * path_hash;
int tam_fs;
int tam_bloque;
int block_count;
char* error_storage="";
char *path_bitmap;





int main(int argc, char* argv[]);
void levantarConfig();
void levantarConfigSuperblock();
void* server_mh_worker(void *args);
char* borrar_directorio(const char* path_a_borrar);
void inicializar_hash();
void inicializar_bloque_fisico(int numero_bloque);
void inicializar_bloques_logicos();
void inicializar_bitmap();


#endif