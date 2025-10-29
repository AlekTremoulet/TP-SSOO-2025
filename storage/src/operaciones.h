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
void Truncar_file(); 

void Crear_tag(); 

void Eliminar_tag(); 

void Copiar_tag();

void Commit_tag(); 

void Escrbir_bloque(); 

void Leer_bloque(); 


#endif