#ifndef OPERACIONES_H
#define OPERACIONES_H

#include <bitmap.h>

typedef enum {
    Inexistente,
    Preexistente,
    Espacio_Insuficiente,
    Escritura_no_Permitida,
    Fuera_Limite
} t_errores_Fs;
typedef struct {
    char * nombre;
    char * hash;
    char * ruta_base;
    char * ruta_tag;
} t_archivo_creado;


typedef enum {
    WIP,
    COMMITED
} t_estado_metadata;

extern char * dir_files;
extern t_bitarray* bitmap;
extern char * dir_physical_blocks;
extern int retardo_bloque;
extern int retardo_operacion;
extern char * path_hash;
//Operaciones


void Crear_file(char* archivo,char* tag,int query_id); 
void Truncar_file(char* archivo, char* tag, int tamanio, int query_id); 
void Escrbir_bloque(char* archivo, char* tag, int dir_base, char* contenido, int query_id); 
char* Leer_bloque(char* archivo, char* tag, int dir_base, int query_id); 
void Eliminar_tag(char* archivo, char* tag, int query_id); 
void Crear_tag(char * Origen,char * Destino,char* tag_origen,char* tag_destino, int query_id);
void Commit_tag(char* archivo, char* tag, int query_id); 
char * crear_archivo_en_FS(char *nombre_archivo, char *tag_archivo);
char* crear_directorio(char* path_a_crear);
char *cargar_archivo(char * ruta_base ,char *ruta_al_archivo);
char *escribir_en_hash(char *nombre_bloque);
void esperar(int milisegundos);

#endif