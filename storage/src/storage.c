#include <storage.h>

char * archivo_config;

int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_worker;

    archivo_config = malloc(strlen(argv[1])+strlen("./")+strlen(".config")+1);

    strcpy(archivo_config, "./");
    strcat(archivo_config, argv[1]);
    strcat(archivo_config, ".config");

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
    for (int i = 0; i < block_count; i++) {
        inicializar_bloque_fisico(i);
    }
    
    pthread_create(&tid_server_mh_worker, NULL, server_mh_worker, NULL);
    pthread_join(tid_server_mh_worker, NULL);

    log_destroy(logger);
    
    free(dir_files);
    free(dir_physical_blocks);
    return 0;
}

void levantarConfig(){

    config = config_create(archivo_config);
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("storage.log", "STORAGE", 1, current_log_level);
    fresh_start = config_get_string_value(config, "FRESH_START");
    punto_montaje = config_get_string_value(config, "PUNTO_MONTAJE");
    retardo_operacion = config_get_int_value(config, "RETARDO_OPERACION");
    retardo_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");

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
    protocolo_socket codigo_respuesta;
    t_list *paquete_recv;
    int socket_nuevo;
    char* archivo;
    char* tag;
    int query_id;
    int tamanio;
    int num_bloque_Log;
    
    parametros_worker parametros_recibidos_worker;

    while((socket_nuevo = esperar_cliente(server))){

        log_info(logger, "Se conecta un Worker con Id: <%d> ",parametros_recibidos_worker.id);
        cod_op = recibir_operacion(socket_nuevo);
        esperar(retardo_operacion);
        switch (cod_op)
        {
        case OP_CREATE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0); 
            tag = list_remove(paquete_recv, 0); 
            query_id = *(int*) list_remove(paquete_recv, 0);
            crear_archivo_en_FS(archivo,tag,query_id);
            enviar_paquete_ok(socket_nuevo);
            break;
        case OP_TRUNCATE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            tamanio = *(int *)list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Truncar_file(archivo,tag,tamanio,query_id);
            break;
        case OP_WRITE:
            esperar(retardo_bloque);
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            num_bloque_Log = *(int *)list_remove(paquete_recv, 0);
            char* contenido = list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Escrbir_bloque(archivo,tag,num_bloque_Log,contenido,query_id);
            break;
        case OP_READ:
            esperar(retardo_bloque);
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            num_bloque_Log = *(int *)list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Leer_bloque(archivo,tag,num_bloque_Log,query_id);
            break;
        case OP_TAG:
            paquete_recv = recibir_paquete(socket_nuevo);
            char* arch_ori = list_remove(paquete_recv, 0);
            char* tag_ori = list_remove(paquete_recv, 0);
            char* arch_dest = list_remove(paquete_recv, 0);
            char* tag_dest = list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Crear_tag(arch_ori,arch_dest,tag_ori,tag_dest,query_id);
            break;
        case OP_COMMIT:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Commit_tag(archivo,tag,query_id);
            break;
        case OP_DELETE:
            paquete_recv = recibir_paquete(socket_nuevo);
            archivo = list_remove(paquete_recv, 0);
            tag = list_remove(paquete_recv, 0);
            query_id = *(int*) list_remove(paquete_recv, 0);
            Eliminar_tag(archivo,tag,query_id);
            break;
        case PARAMETROS_STORAGE:
            paquete_recv = recibir_paquete(socket_nuevo);
            list_destroy_and_destroy_elements(paquete_recv, free);
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
        if(strcmp(error_storage,"")){
            t_paquete* paquete = crear_paquete(ERR_ESCRITURA_ARCHIVO_COMMITED);
            agregar_a_paquete(paquete, error_storage, strlen(error_storage) + 1);
            enviar_paquete(paquete, socket_nuevo);
            eliminar_paquete(paquete);
            error_storage="";
        }
        else{
            enviar_paquete_ok(socket_nuevo);
        }
        
           
    }
    return (void *)EXIT_SUCCESS;
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


void inicializar_hash() {
    path_hash = cargar_archivo(punto_montaje,"/blocks_hash_index.config");
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
}

void inicializar_bloques_logicos(){
    char * Base = crear_archivo_en_FS("initial_file","BASE",0); //falta ver que este archivo NO se pueda borrar
    char *dir_logical_base = malloc(strlen(Base) + strlen("/000000.dat") + 1);
    char *dir_phisical_base = malloc(strlen(dir_physical_blocks) + strlen("/block0000.dat") + 1);

    sprintf(dir_logical_base ,"%s/%s", Base,"000000.dat");
    sprintf(dir_phisical_base ,"%s/%s", dir_physical_blocks,"block0000.dat");
    ocupar_espacio_bitmap(0);
    char *nombre_archivo = malloc(20);
    sprintf(nombre_archivo, "/block%04d", 0);
    escribir_en_hash(nombre_archivo);
    link(dir_phisical_base,dir_logical_base); //ESTO ESTA MAL >:( (Hardcodeado)
    
}
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