#include <storage.h>

int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_worker;

    levantarConfig();
    if (strcmp(fresh_start, "TRUE") == 0){
        borrar_directorio(punto_montaje);
        crear_directorio(punto_montaje);
    }else{
        crear_directorio(punto_montaje);
    }

    pthread_create(&tid_server_mh_worker, NULL, server_mh_worker, NULL);
    pthread_join(tid_server_mh_worker, NULL);

    log_destroy(logger);
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