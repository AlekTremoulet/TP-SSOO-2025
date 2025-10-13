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
    crear_directorio(dir_files);
    crear_directorio(dir_physical_blocks);

    levantarConfigSuperblock();
    inicializar_bitmap();
    inicializar_hash();
    inicializar_bloques_fisicos();
    inicializar_bloques_logicos();
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

    tam_fs = config_get_int_value(config, "FS_SIZE");
    tam_bloque = config_get_int_value(config, "BLOCK_SIZE");
    block_count = tam_fs / tam_bloque;
    char *superblock = cargar_archivo(punto_montaje,"/superblock.config");
    FILE *fp = fopen(superblock, "wb+");
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


char *cargar_archivo(char * ruta_base ,char *ruta_al_archivo){ 
    size_t path_length = strlen(ruta_base) + strlen(ruta_al_archivo) + 2;
    char *path_creado = malloc(path_length);
    if (!path_creado) {
        log_info(logger, "Error: No se pudo asignar memoria para crear el archivo");
        exit(EXIT_FAILURE);
    }
    
    if (!strcmp(ruta_base, "/")!= 0) {
        snprintf(path_creado, path_length, "%s/%s", ruta_base,ruta_al_archivo);
    } else {
        snprintf(path_creado, path_length, "%s%s", ruta_base,ruta_al_archivo);
    }
    log_debug(logger, "Ruta del archivo: %s", path_creado);
    log_debug(logger, "archivo %s inicializado correctamente.",path_creado);

    return path_creado;
}

void inicializar_hash() {
    path_hash = cargar_archivo(punto_montaje,"/blocks_hash_index.config");
    FILE *hash_file = fopen(path_hash, "wb+");
    if (!hash_file) {
        log_error(logger,"Error al abrir blocks_hash_index.config");
        exit(EXIT_FAILURE);
    }
    fclose(hash_file);
}

char *escribir_en_hash(char *nombre_bloque) {
    FILE * hash_file = fopen(path_hash, "ab+");
    if (!hash_file) {
        log_error(logger,"Error al abrir blocks_hash_index.config");
        exit(EXIT_FAILURE);
    }
    size_t longitud_bloque = strlen(nombre_bloque);
    char *nombre_bloque_hash = crypto_md5(nombre_bloque, longitud_bloque);
    nombre_bloque = nombre_bloque + 1; //Omitir el primer caracter
    fprintf(hash_file, "%s=%s\n",nombre_bloque_hash,nombre_bloque);
    fclose(hash_file);
    return nombre_bloque_hash;
}

char * crear_archivo_en_FS(char *nombre_archivo, char *tag_archivo) {
    t_archivo_creado archivo;
    archivo.nombre = nombre_archivo;
    archivo.ruta_base= malloc(strlen(dir_files) + strlen("/") + strlen(nombre_archivo)+ 1); //NO SE DONDE IRIA UN FREE
    sprintf(archivo.ruta_base, "%s/%s", dir_files,nombre_archivo);

    archivo.ruta_tag= malloc(strlen(archivo.ruta_base) + strlen("/") + strlen(tag_archivo)+ 1); //NO SE DONDE IRIA UN FREE
    sprintf(archivo.ruta_tag, "%s/%s", archivo.ruta_base,tag_archivo);

    char* directorio_base = crear_directorio(archivo.ruta_base);
    char* directorio_tag = crear_directorio(archivo.ruta_tag);
    log_info(logger,"La ruta base de %s es %s",archivo.nombre,archivo.ruta_base);
    log_info(logger,"La ruta al tag es%s ",archivo.ruta_tag);

    char *config_asociada = malloc(strlen(directorio_tag) + strlen("/metadata.config") + 1); //NO SE DONDE IRIA UN FREE
    sprintf(config_asociada, "%s/%s", directorio_tag,"metadata.config");

    char *directorio_config_asociada = cargar_archivo("",config_asociada);
    log_info(logger,"directorio_config_asociada %s ",directorio_config_asociada);

    
    // ESTO LO VOY A HACER A PARTE PERO ME DA FIACA HACERLO AHORA
    t_bloque_fisico Bloque_F; 
    Bloque_F.Estado = "WORK IN PROGRESS";
    Bloque_F.Blocks = "[]"; // getBlockesMetadata();
    Bloque_F.Tamanno = "0"; // getTamannoMetadata();

    FILE * config_archivo_metadata = fopen(directorio_config_asociada, "wb+");
    if (!config_archivo_metadata) {
        log_error(logger,"Error al abrir el .config");
        exit(EXIT_FAILURE);
    }
    fprintf(config_archivo_metadata, "ESTADO=%s\n",Bloque_F.Estado);
    fprintf(config_archivo_metadata, "Blocks=%s\n",Bloque_F.Blocks);
    fprintf(config_archivo_metadata, "Tamaño=%s\n",Bloque_F.Tamanno);

    fclose(config_archivo_metadata);
    char *dir_logical_blocks = malloc(strlen(directorio_tag) + strlen("/logical_blocks") + 1);
    sprintf(dir_logical_blocks, "%s/%s", directorio_tag,"logical_blocks");
    crear_directorio(dir_logical_blocks);

    return dir_logical_blocks;
}

void inicializar_bloques_fisicos(){
    for (int i = 0; i < block_count; i++) {
        char *nombre_archivo = malloc(20);
        sprintf(nombre_archivo, "/block%04d.dat", i);

        char *path_block_dat = cargar_archivo(dir_physical_blocks, nombre_archivo);;
        FILE *block_dat_file = fopen(path_block_dat, "wb+");
        if (!block_dat_file) {
            log_error(logger,"Error al abrir blocks_hash_index.config");
            exit(EXIT_FAILURE);
        }
        fclose(block_dat_file);
        escribir_en_hash(nombre_archivo);

    }
}

void inicializar_bloques_logicos(){
    char * Base = crear_archivo_en_FS("initial_file","BASE"); 
    char *dir_logical_base = malloc(strlen(Base) + strlen("/000000.dat") + 1);
    char *dir_phisical_base = malloc(strlen(dir_physical_blocks) + strlen("/block0000.dat") + 1);

    sprintf(dir_logical_base ,"%s/%s", Base,"000000.dat");
    sprintf(dir_phisical_base ,"%s/%s", dir_physical_blocks,"block0000.dat");

    link(dir_phisical_base,dir_logical_base); //ESTO ESTA MAL >:( (Hardcodeado)
    
}
