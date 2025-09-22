#ifndef BITMAP_H
#define BITMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <utils/utils.h>
#include <math.h>

extern char * punto_montaje;
extern int tam_fs;
extern int tam_bloque;
extern int block_count;

static t_bitarray* bitmap;
static FILE* bitmap_file;
extern t_log *logger;

int libres;

uint32_t bits_ocupados;
char *path_bitmap;

typedef struct {
    uint32_t bloque_indice;
    uint32_t cantidad_bloques;
    t_list* lista_indices;
} t_reserva_bloques;



void inicializar_bitmap();
bool espacio_disponible(uint32_t cantidad);
void liberar_bloque(uint32_t bloque);
int cargar_bitmap();
void destruir_bitmap();
char *cargar_en_fs(char *ruta_al_archivo);

#endif
