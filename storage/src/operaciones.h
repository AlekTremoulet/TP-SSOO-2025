#ifndef OPERACIONES_H
#define OPERACIONES_H


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


typedef enum {
    Inexistente,
    Preexistente,
    Espacio_Insuficiente,
    Escritura_no_Permitida,
    Fuera_Limite
} t_errores_Fs;


//Operaciones


void Crear_file(char* archivo,char* tag); 
void Truncar_file(char* archivo, char* tag, int tamanio, int query_id); 
void Escrbir_bloque(char* archivo, char* tag, int dir_base, char* contenido, int query_id); 
void Leer_bloque(char* archivo, char* tag, int dir_base, int tamanio, int query_id); 
void Eliminar_tag(char* archivo, char* tag, int query_id); 
void Copiar_tag(char * Origen,char * Destino,char* tag_origen,char* tag_destino);
void Commit_tag(char* archivo, char* tag, int query_id); 




#endif