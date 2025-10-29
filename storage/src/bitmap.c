#include "bitmap.h"
 
void inicializar_bitmap() {
    size_t tamanio_bitmap = (size_t)ceil((double)block_count/8);;
    
    path_bitmap = cargar_ruta("bitmap.bin");


    bitmap_file = fopen(path_bitmap, "rb+");
    if (!bitmap_file) {
        bitmap_file = fopen(path_bitmap, "wb+");
        if (!bitmap_file) {
            log_info(logger, "Error al crear el archivo bitmap.bin");
            free(path_bitmap);
            exit(EXIT_FAILURE);
        }
        uint8_t* buffer = calloc(tamanio_bitmap, sizeof(uint8_t));
        if (!buffer) {
            log_info(logger, "Error al asignar memoria para el buffer inicial.");
            fclose(bitmap_file);
            free(path_bitmap);
            exit(EXIT_FAILURE);
        }
        if (fwrite(buffer, sizeof(uint8_t), tamanio_bitmap, bitmap_file) != tamanio_bitmap) {
            log_info(logger, "Error al escribir en el archivo bitmap.bin");
            free(buffer);
            fclose(bitmap_file);
            free(path_bitmap);
            exit(EXIT_FAILURE);
        }
        fflush(bitmap_file);
        free(buffer);
    }

    uint8_t* contenido_bitmap = malloc(tamanio_bitmap);
    if (!contenido_bitmap) {
        log_info(logger, "Error al asignar memoria para contenido_bitmap");
        fclose(bitmap_file);
        free(path_bitmap);
        exit(EXIT_FAILURE);
    }
    rewind(bitmap_file);
    if (fread(contenido_bitmap, sizeof(uint8_t), tamanio_bitmap, bitmap_file) != tamanio_bitmap) {
        log_info(logger, "Error al leer el archivo bitmap.bin");
        free(contenido_bitmap);
        fclose(bitmap_file);
        free(path_bitmap);
        exit(EXIT_FAILURE);
    }


    bitmap = bitarray_create_with_mode((char*)contenido_bitmap, tamanio_bitmap, LSB_FIRST);
    if (!bitmap) {
        log_info(logger, "Error al inicializar el bitmap.");
        free(contenido_bitmap);
        fclose(bitmap_file);
        free(path_bitmap);
        exit(EXIT_FAILURE);
    }
    log_info(logger, "Bitmap inicializado correctamente.");

}

int espacio_disponible(t_bitarray * bitmap) {
    for (int i = 0; i < bitarray_get_max_bit(bitmap) ; i++){
        if (bitarray_test_bit(bitmap,i) != 1) return i;
    }
    return -1;
}

int ocupar_espacio_bitmap(int offset_bit) {

    FILE* bitmap_file = fopen(path_bitmap, "rb+");
    if (bitmap_file == NULL) {
        log_info(logger, "Error al abrir el archivo bitmap.bin para escritura.");
        return -1;
    }

    rewind(bitmap_file);
    size_t bytes_bitmap = block_count / 8;
    bitarray_set_bit(bitmap,offset_bit);

    if (fwrite(bitmap->bitarray, bytes_bitmap, 1, bitmap_file) != 1) {
        log_info(logger, "Error al escribir el bitmap en bitmap.bin");
        fclose(bitmap_file);
        return -1;
    }
    fclose(bitmap_file);
    
    return 0;
}

int liberar_espacio_bitmap(int offset_bit) {

    FILE* bitmap_file = fopen(path_bitmap, "rb+");
    if (bitmap_file == NULL) {
        log_info(logger, "Error al abrir el archivo bitmap.bin para escritura.");
        return -1;
    }

    rewind(bitmap_file);
    size_t bytes_bitmap = block_count / 8;
    bitarray_clean_bit(bitmap,offset_bit);

    if (fwrite(bitmap->bitarray, bytes_bitmap, 1, bitmap_file) != 1) {
        log_info(logger, "Error al escribir el bitmap en bitmap.bin");
        fclose(bitmap_file);
        return -1;
    }
    fclose(bitmap_file);
    
    return 0;
}

void destruir_bitmap() {
    if (!bitmap_file) {
        log_info(logger, "Error al abrir el archivo bitmap.bin para escribir");
        exit(EXIT_FAILURE);
    }

    fseek(bitmap_file, 0, SEEK_SET);
    size_t bytes_a_escribir = (bitarray_get_max_bit(bitmap) + 7) / 8;
    if (fwrite(bitmap->bitarray, sizeof(uint8_t), bytes_a_escribir, bitmap_file) != bytes_a_escribir) {
        log_info(logger, "Error al escribir el archivo bitmap.bin");
        exit(EXIT_FAILURE);
    }
    fflush(bitmap_file);

    bitarray_destroy(bitmap);
    fclose(bitmap_file);

}

char *cargar_ruta(char *ruta_al_archivo){ 
    size_t path_length = strlen(punto_montaje) + strlen(ruta_al_archivo) + 2;
    char *path_creado = malloc(path_length);
    if (!path_creado) {
        log_info(logger, "Error: No se pudo asignar memoria para crear el archivo");
        exit(EXIT_FAILURE);
    }
    
    snprintf(path_creado, path_length, "%s/%s", punto_montaje,ruta_al_archivo);
    log_info(logger, "Ruta del archivo: %s", path_creado);
    log_info(logger, "archivo %s inicializado correctamente.",path_creado);

    return path_creado;
}

void buscar_bit_libre(){
    if (espacio_disponible(bitmap) !=-1){
        ocupar_espacio_bitmap(espacio_disponible(bitmap));
    } else{
        ocupar_espacio_bitmap(libres);
        libres++;
        if (libres > bitarray_get_max_bit(bitmap)) libres = 0; 
    }
};