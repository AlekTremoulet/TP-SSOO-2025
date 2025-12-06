#ifndef BITMAP_H
#define BITMAP_H

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
#include <commons/bitarray.h>

#include <pthread.h>
#include <math.h>

extern char * punto_montaje;
extern int tam_fs;
extern int tam_bloque;
extern int block_count;


extern t_log *logger;

extern char *path_bitmap;

typedef struct {
    uint32_t bloque_indice;
    uint32_t cantidad_bloques;
    t_list* lista_indices;
} t_reserva_bloques;



int espacio_disponible(t_bitarray * bitmap);
int ocupar_espacio_bitmap(int offset_bit);
int liberar_espacio_bitmap(int offset_bit);
void destruir_bitmap();
char *cargar_ruta(char *ruta_al_archivo);
void buscar_y_ocupar_siguiente_bit_libre(int siguiente_bit_libre);

#endif
