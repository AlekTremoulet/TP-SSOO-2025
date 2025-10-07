#include <storage.h>

int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_worker;

    levantarConfig();
    if (strcmp(fresh_start, "TRUE") == 0){
        borrar_directorio(punto_montaje);
        crear_directorio(punto_montaje);
        crear_directorio(dir_files);
        crear_directorio(dir_physical_blocks);
    }else{
        crear_directorio(punto_montaje);
        crear_directorio(dir_files);
        crear_directorio(dir_physical_blocks);
    }
    levantarConfigSuperblock();
    inicializar_bitmap();
    inicializar_hash();
    pthread_create(&tid_server_mh_worker, NULL, server_mh_worker, NULL);
    pthread_join(tid_server_mh_worker, NULL);

    log_destroy(logger);
    free(dir_files);
    free(dir_physical_blocks);
    return 0;
}

void levantarConfig(){

    config = config_create("./storage.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("storage.log", "STORAGE", 1, current_log_level);
    fresh_start = config_get_string_value(config, "FRESH_START");
    punto_montaje = config_get_string_value(config, "PUNTO_MONTAJE");

    dir_files = malloc(strlen(punto_montaje) + strlen("/files") + 1);
    sprintf(dir_files, "%s/files", punto_montaje);

    dir_physical_blocks = malloc(strlen(punto_montaje) + strlen("/physical_blocks") + 1);
    sprintf(dir_physical_blocks, "%s/physical_blocks", punto_montaje);
}


void levantarConfigSuperblock(){
    configSuperblock = config_create("./superblock.config");
    tam_fs = config_get_int_value(configSuperblock, "FS_SIZE");
    tam_bloque = config_get_int_value(configSuperblock, "BLOCK_SIZE");
    block_count = tam_fs / tam_bloque;
    char *superblock = cargar_archivo("superblock.config");
    FILE *fp = fopen(superblock, "w");
    if (!fp) {
        log_error(logger,"Error al abrir superblock.config");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "FS_SIZE=%d\n", tam_fs);
    fprintf(fp, "BLOCK_SIZE=%d\n", tam_bloque);
    fclose(fp);
    
}


void *server_mh_worker(void *args){ // Server Multi-hilo 
    int server = iniciar_servidor(puerto);
    protocolo_socket cod_op;
    t_list *paquete_recv;
    int socket_nuevo;
    parametros_worker parametros_recibidos_worker;

    while((socket_nuevo = esperar_cliente(server))){
        
        cod_op = recibir_operacion(socket_nuevo);
        if(cod_op != PARAMETROS_WORKER){
            log_error(logger, "Se recibio un protocolo inesperado de WORKER");
            return (void *)EXIT_SUCCESS;
            }
        paquete_recv = recibir_paquete(socket_nuevo);
        int* id_ptr = list_remove(paquete_recv, 0);
        parametros_recibidos_worker.id = *id_ptr;
        log_info(logger, "Se conecta un Worker con Id: <%d> ",parametros_recibidos_worker.id);
        enviar_paquete_ok(socket_nuevo);
           
    }
    return (void *)EXIT_SUCCESS;
}

char* crear_directorio(char* path_a_crear) {
    if (!path_a_crear) {
        log_error(logger, "Error: La ruta base es NULL.\n");
        return NULL;
    }

    char* ruta = strdup(path_a_crear);
    if (!ruta) {
        log_error(logger, "Error: No se pudo asignar memoria para la ruta del directorio.\n");
        return NULL;
    }

    if (mkdir(ruta, 0700) == 0) {
        log_debug(logger,"Directorio '%s' creado correctamente.\n", ruta);
    } else if (errno == EEXIST) {
        log_debug(logger,"El directorio '%s' ya existe.\n", ruta);
    } else {
        log_error(logger,"Error al crear el directorio");
        free(ruta);
        return NULL;
    }

    return ruta;
}

char* borrar_directorio(const char* path_a_borrar) {
    if (!path_a_borrar) {
        log_error(logger, "Error: La ruta a borrar es NULL.\n");
        return NULL;
    }

    DIR* dir = opendir(path_a_borrar);
    if (!dir) {
        if (errno == ENOENT) {
            log_debug(logger, "El directorio '%s' no existe.\n", path_a_borrar);
        } else {
            log_error(logger, "No se pudo abrir el directorio '%s'.\n", path_a_borrar);
        }
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (strcmp(entry->d_name, "superblock.config") == 0)
            continue;
        char* ruta_completa = malloc(strlen(path_a_borrar) + strlen(entry->d_name) + 2);
        sprintf(ruta_completa, "%s/%s", path_a_borrar, entry->d_name);

        struct stat st;
        if (stat(ruta_completa, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                borrar_directorio(ruta_completa);
                if (rmdir(ruta_completa) != 0) {
                    log_error(logger, "No se pudo borrar el directorio '%s'.\n", ruta_completa);
                } else {
                    log_debug(logger, "Directorio '%s' borrado.\n", ruta_completa);
                }
            } else {
                if (remove(ruta_completa) != 0) {
                    log_error(logger, "No se pudo borrar el archivo '%s'.\n", ruta_completa);
                } else {
                    log_debug(logger, "Archivo '%s' borrado.\n", ruta_completa);
                }
            }
        }
        free(ruta_completa);
    }
    closedir(dir);
    return strdup(path_a_borrar);
}


char *cargar_archivo(char *ruta_al_archivo){ 
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

void inicializar_hash() {

    char *path_hash = cargar_archivo("blocks_hash_index.config");
    FILE *hash_file = fopen(path_hash, "wb+");
    if (!hash_file) {
        log_error(logger,"Error al abrir blocks_hash_index.config");
        exit(EXIT_FAILURE);
    }
    fprintf(hash_file, "hola");
    fclose(hash_file);


}
