#include <storage.h>
#include <operaciones.h>

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
    inicializar_bloques_logicos();
    Crear_file("archivo","tag",1);
    Truncar_file("archivo","tag",10,1);    
    Escrbir_bloque("archivo","tag",1,"hola",3);
    Escrbir_bloque("archivo","tag",1,"hola",3);
    // Crear_tag("/home/utnso/storage/files/initial_file","/home/utnso/storage/files/initial_file2");
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
    char* archivo;
    char* tag;
    int query_id;
    int* tamanio;
    int* dir_base;
    parametros_worker parametros_recibidos_worker;

    while((socket_nuevo = esperar_cliente(server))){

        log_info(logger, "Se conecta un Worker con Id: <%d> ",parametros_recibidos_worker.id);
        cod_op = recibir_operacion(socket_nuevo);
        switch (cod_op)
        {
        case OP_CREATE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0); 
            tag = list_remove(paquete_recv, 0); 
            query_id = list_remove(paquete_recv, 0);
            Crear_file(archivo,tag,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_TRUNCATE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            tamanio = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Truncar_file(archivo,tag,tamanio,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_WRITE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            int* dir_base = list_remove(paquete_recv, 0);
            char* contenido = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Escrbir_bloque(archivo,tag,dir_base,contenido,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_READ:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            dir_base = list_remove(paquete_recv, 0);
            tamanio = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Leer_bloque(archivo,tag,dir_base,tamanio,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_TAG:
            paquete_recv = recibir_paquete(socket_nuevo);
            char* arch_ori = list_remove(paquete_recv, 0);
            char* tag_ori = list_remove(paquete_recv, 0);
            char* arch_dest = list_remove(paquete_recv, 0);
            char* tag_dest = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Crear_tag(tag_ori,arch_dest,tag_ori,tag_dest,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_COMMIT:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Commit_tag(archivo,tag,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_FLUSH:
            //cuando este listo memoria vemos esto
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_DELETE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            query_id = list_remove(paquete_recv, 0);
            Eliminar_tag(archivo,tag,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case PARAMETROS_STORAGE:
                t_paquete* paquete_send = crear_paquete(PARAMETROS_STORAGE);
                agregar_a_paquete(paquete_send,&tam_bloque,sizeof(int));
                enviar_paquete(paquete_send,socket_nuevo);
                eliminar_paquete(paquete_send);
            break;
        default:
            log_error(logger, "Se recibio un protocolo inesperado de WORKER");
            return (void *)EXIT_FAILURE;
            break;
        }

           
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
    t_config *hash_config = config_create(path_hash);

    if (!hash_config) {
        FILE *temp_file = fopen(path_hash, "w");
        if (temp_file) {
            fclose(temp_file);
            hash_config = config_create(path_hash); 
        }

        if (!hash_config) { 
            log_error(logger, "Error al crear/abrir blocks_hash_index.config en %s", path_hash);
            exit(EXIT_FAILURE);
        }
    }

    size_t longitud_bloque = strlen(nombre_bloque);
    char *nombre_bloque_hash = crypto_md5(nombre_bloque, longitud_bloque); 
    char *nombre_bloque_sin_prefijo = nombre_bloque + 1; 
    config_set_value(hash_config, nombre_bloque_hash, nombre_bloque_sin_prefijo);

    if (config_save(hash_config) == -1) {
        log_error(logger, "Error al guardar los datos en blocks_hash_index.config");
    }
    config_destroy(hash_config);
    return nombre_bloque_hash; 
}

char * crear_archivo_en_FS(char *nombre_archivo, char *tag_archivo) { //Operacion de crear archivo
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
    Bloque_F.Tamanio = "0"; // getTamannoMetadata();

    t_config *config_archivo_metadata = config_create(directorio_config_asociada);
    if (!config_archivo_metadata) {
        FILE *temp_file = fopen(directorio_config_asociada, "w");
        if (temp_file) {
            fclose(temp_file);
            config_archivo_metadata = config_create(directorio_config_asociada); 
        }

        if (!config_archivo_metadata) { 
            exit(EXIT_FAILURE);
        }
    }
    
    config_set_value(config_archivo_metadata, "ESTADO", Bloque_F.Estado);
    config_set_value(config_archivo_metadata, "Blocks", Bloque_F.Blocks);
    config_set_value(config_archivo_metadata, "Tamanio", Bloque_F.Tamanio);

    if (config_save(config_archivo_metadata) == -1) {
        log_error(logger, "Error al guardar los datos en el .config: %s", directorio_config_asociada);
    }
    
    
    char *dir_logical_blocks = malloc(strlen(directorio_tag) + strlen("/logical_blocks") + 1);
    sprintf(dir_logical_blocks, "%s/%s", directorio_tag,"logical_blocks");
    crear_directorio(dir_logical_blocks);

    return dir_logical_blocks;
}

void inicializar_bloque_fisico(int numero_bloque){
    char *nombre_archivo = malloc(20);
    sprintf(nombre_archivo, "/block%04d.dat", numero_bloque);

    char *path_block_dat = cargar_archivo(dir_physical_blocks, nombre_archivo);;
    FILE *block_dat_file = fopen(path_block_dat, "wb+");
    if (!block_dat_file) {
        log_error(logger,"Error al abrir blocks_hash_index.config");
        exit(EXIT_FAILURE);
    }
    fclose(block_dat_file);
    escribir_en_hash(nombre_archivo);
}

void inicializar_bloques_logicos(){
    char * Base = crear_archivo_en_FS("initial_file","BASE"); 
    char *dir_logical_base = malloc(strlen(Base) + strlen("/000000.dat") + 1);
    char *dir_phisical_base = malloc(strlen(dir_physical_blocks) + strlen("/block0000.dat") + 1);

    sprintf(dir_logical_base ,"%s/%s", Base,"000000.dat");
    sprintf(dir_phisical_base ,"%s/%s", dir_physical_blocks,"block0000.dat");
    ocupar_espacio_bitmap(0);
    inicializar_bloque_fisico(0);
    link(dir_phisical_base,dir_logical_base); //ESTO ESTA MAL >:( (Hardcodeado)
    
}
