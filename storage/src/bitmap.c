#include "bitmap.h"

t_bitarray* bitmap;
FILE* bitmap_file;
int libres = 0;
uint32_t bits_ocupados;

int espacio_disponible(t_bitarray * bitmap) {
    for (int i = 0; i < bitarray_get_max_bit(bitmap) ; i++){
        if (bitarray_test_bit(bitmap,i) != 1) return i;
    }
    return -1;
}

int ocupar_espacio_bitmap(int offset_bit) {

    FILE* bitmap_file = fopen(path_bitmap, "rb+");
    if (bitmap_file == NULL) {
        log_error(logger, "Error al abrir el archivo bitmap.bin para escritura.");
        return -1;
    }

    rewind(bitmap_file);
    size_t bytes_bitmap = block_count / 8;
    bitarray_set_bit(bitmap,offset_bit);

    if (fwrite(bitmap->bitarray, bytes_bitmap, 1, bitmap_file) != 1) {
        log_error(logger, "Error al escribir el bitmap en bitmap.bin");
        fclose(bitmap_file);
        return -1;
    }
    fclose(bitmap_file);
    
    return 0;
}

int liberar_espacio_bitmap(int offset_bit) {

    FILE* bitmap_file = fopen(path_bitmap, "rb+");
    if (bitmap_file == NULL) {
        log_error(logger, "Error al abrir el archivo bitmap.bin para escritura.");
        return -1;
    }

    rewind(bitmap_file);
    size_t bytes_bitmap = block_count / 8;
    bitarray_clean_bit(bitmap,offset_bit);

    if (fwrite(bitmap->bitarray, bytes_bitmap, 1, bitmap_file) != 1) {
        log_error(logger, "Error al escribir el bitmap en bitmap.bin");
        fclose(bitmap_file);
        return -1;
    }
    fclose(bitmap_file);
    
    return 0;
}

void destruir_bitmap() {
    if (!bitmap_file) {
        log_error(logger, "Error al abrir el archivo bitmap.bin para escribir");
        exit(EXIT_FAILURE);
    }

    fseek(bitmap_file, 0, SEEK_SET);
    size_t bytes_a_escribir = (bitarray_get_max_bit(bitmap) + 7) / 8;
    if (fwrite(bitmap->bitarray, sizeof(uint8_t), bytes_a_escribir, bitmap_file) != bytes_a_escribir) {
        log_error(logger, "Error al escribir el archivo bitmap.bin");
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
        log_error(logger, "Error: No se pudo asignar memoria para crear el archivo");
        exit(EXIT_FAILURE);
    }
    
    snprintf(path_creado, path_length, "%s/%s", punto_montaje,ruta_al_archivo);
    log_debug(logger, "Ruta del archivo: %s", path_creado);
    log_debug(logger, "archivo %s inicializado correctamente.",path_creado);

    return path_creado;
}

void buscar_y_ocupar_siguiente_bit_libre(int Siguiente_bit_libre){
    if (Siguiente_bit_libre !=-1){
        ocupar_espacio_bitmap(Siguiente_bit_libre);
    } else{
        ocupar_espacio_bitmap(libres);
        libres++;
        if (libres > bitarray_get_max_bit(bitmap)) libres = 0; 
    }
};