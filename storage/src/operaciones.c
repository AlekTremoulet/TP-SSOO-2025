#include "operaciones.h"

char * crear_archivo_en_FS(char *nombre_archivo, char *tag_archivo) {
    t_archivo_creado archivo;
    archivo.nombre = nombre_archivo;
    archivo.ruta_base= malloc(strlen(dir_files) + strlen("/") + strlen(nombre_archivo)+ 1); //NO SE DONDE IRIA UN FREE
    sprintf(archivo.ruta_base, "%s/%s", dir_files,nombre_archivo);

    DIR* dir = opendir(archivo.ruta_base);
    if (dir) {
        closedir(dir);
        return "Error_create";
    }
    
    closedir(dir);
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



    char *Estado = "WORK IN PROGRESS";
    char *Blocks = "[]";
    char *Tamanio = "0";

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
    
    config_set_value(config_archivo_metadata, "ESTADO", Estado);
    config_set_value(config_archivo_metadata, "Blocks", Blocks);
    config_set_value(config_archivo_metadata, "Tamanio", Tamanio);

    if (config_save(config_archivo_metadata) == -1) {
        log_error(logger, "Error al guardar los datos en el .config: %s", directorio_config_asociada);
    }
    
    
    char *dir_logical_blocks = malloc(strlen(directorio_tag) + strlen("/logical_blocks") + 1);
    sprintf(dir_logical_blocks, "%s/%s", directorio_tag,"logical_blocks");
    crear_directorio(dir_logical_blocks);

    return dir_logical_blocks;
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


void Crear_file(char* archivo,char* tag, int query_id, protocolo_socket * error){
    char * crear_archivo_status = crear_archivo_en_FS(archivo, tag);
    if (!strcmp(crear_archivo_status,"Error_create")){
        log_error(logger,"Error al crear el archivo");
        *error = ERR_FILE_PREEXISTENTE;
    } else {
        log_info(logger,"<%d> - File Creado <%s>:<%s>",query_id,archivo,tag);
    }
}; 

void Truncar_file(char* archivo, char* tag, int tamanio, int query_id, protocolo_socket * error) 
{
    tamanio /= tam_bloque;
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config", dir_files, archivo, tag);

    t_config *config_a_truncar = config_create(metadata_config_asociado);
    int tamanio_actual = config_get_int_value(config_a_truncar, "Tamanio");
    char *estado_actual = config_get_string_value(config_a_truncar, "ESTADO");
    config_set_value(config_a_truncar, "Tamanio", string_itoa(tamanio));

    if (strcmp(estado_actual, "COMMITED") == 0){
        log_error(logger, "Error, ESCRITURA NO PERMITIDA (ARCHIVO COMITEADO)");
        *error = ERR_ESCRITURA_ARCHIVO_COMMITED;
        free(metadata_config_asociado);
    }
    else
    {
        char** bloques_actuales = config_get_array_value(config_a_truncar, "Blocks");
        char* lista_final = string_new();

        char* archivo_tag = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 5);
        sprintf(archivo_tag, "%s/%s/%s", dir_files, archivo, tag);

        string_append(&lista_final, "[");

        for (int i = 0; bloques_actuales[i] != NULL; i++) {
            if (i > 0) {
                string_append(&lista_final, ",");
            }
            string_append(&lista_final, bloques_actuales[i]);
        }

        if (tamanio > tamanio_actual) {
            int tamanio_diferencia = tamanio - tamanio_actual;
            int contador_bloques = 0;
            while(bloques_actuales[contador_bloques] != NULL) {
                contador_bloques++;
            }

            for (int i = 0; i < tamanio_diferencia; i++) {
                
                int bloque_reservado = 0; 
                int numero_nuevo_bloque = contador_bloques + i;

                if (strlen(lista_final) > 1) { 
                    string_append(&lista_final, ",");
                } 
                string_append_with_format(&lista_final, "%d", bloque_reservado);
                
                char* logical_blocks_archivo_tagi = malloc(strlen(archivo_tag) + 50);
                sprintf(logical_blocks_archivo_tagi, "%s/logical_blocks/%06d.dat", archivo_tag, numero_nuevo_bloque);
                
                char* bloque_archivo = malloc(strlen(dir_physical_blocks) + 20);
                sprintf(bloque_archivo, "%s/block%04d.dat", dir_physical_blocks, bloque_reservado);
                
                link(bloque_archivo, logical_blocks_archivo_tagi);
                
                log_info(logger, "<%d> - <%s>:<%s> Se agregó el hard link del bloque lógico <%s> al bloque físico <%s>", query_id, archivo, tag, logical_blocks_archivo_tagi, bloque_archivo);
                
                free(logical_blocks_archivo_tagi);
                free(bloque_archivo);
            }
            
        } else if (tamanio < tamanio_actual) { 
            int tamanio_diferencia = tamanio_actual - tamanio;
            int cantidad_bloques = 0;
            
            while(bloques_actuales[cantidad_bloques] != NULL) {
                cantidad_bloques++;
            }

            free(lista_final);
            lista_final = string_new();
            string_append(&lista_final, "[");

            for (int i = 0; i < cantidad_bloques; i++) {
                int bloque_id = atoi(bloques_actuales[i]);
                
                if (i < (cantidad_bloques - tamanio_diferencia)) {
                    if (i > 0) {
                        string_append(&lista_final, ",");
                    }
                    string_append(&lista_final, bloques_actuales[i]);
                } else {
                    char* logical_blocks_archivo_tagi = malloc(strlen(archivo_tag) + 50);
                    sprintf(logical_blocks_archivo_tagi, "%s/logical_blocks/%06d.dat", archivo_tag, i); 
                    
                    unlink(logical_blocks_archivo_tagi);
                    log_info(logger, "<%d> - Borrando hard link lógico: %s", query_id, logical_blocks_archivo_tagi);
                    free(logical_blocks_archivo_tagi);
                    char* bloque_fisico_path = malloc(strlen(dir_physical_blocks) + 20);
                    sprintf(bloque_fisico_path, "%s/block%04d.dat", dir_physical_blocks, bloque_id);
                    
                    struct stat st;
                    if (stat(bloque_fisico_path, &st) == 0) {
                        if (st.st_nlink == 1) {
                            bitarray_clean_bit(bitmap, bloque_id);
                            log_info(logger, "<%d> - Bloque Liberado (Nlink=1): %d", query_id, bloque_id);
                        } else {
                            log_info(logger, "<%d> - Bloque NO Liberado (Compartido):l %d", query_id, bloque_id);
                        }
                    }
                    free(bloque_fisico_path);
                }
            }
        }

        string_append(&lista_final, "]");
        config_set_value(config_a_truncar, "Blocks", lista_final);
        config_save(config_a_truncar);
        string_iterate_lines(bloques_actuales, (void*) free);
        free(bloques_actuales);
        free(lista_final);
        free(archivo_tag);
        config_destroy(config_a_truncar); 
        free(metadata_config_asociado);   

        log_info(logger, "<%d> - File Truncado <%s>:<%s> - Tamaño: <%d>", query_id, archivo, tag, tamanio);
    }
}

void Escrbir_bloque(char* archivo, char* tag, int num_bloque_Log, char* contenido, int query_id, protocolo_socket * error){
    
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config", dir_files, archivo, tag);

    t_config *config_a_escribir = config_create(metadata_config_asociado);
    
    if (config_a_escribir == NULL) {
        log_error(logger, "Error: No se pudo abrir la metadata en %s", metadata_config_asociado);
        *error = ERR_METADATA;
        free(metadata_config_asociado);
        return;
    }

    char *estado_actual = config_get_string_value(config_a_escribir, "ESTADO");

    if (strcmp(estado_actual, "COMMITED") == 0){
        log_error(logger, "Error, ESCRITURA NO PERMITIDA (ARCHIVO COMITEADO)");
        *error = ERR_ESCRITURA_ARCHIVO_COMMITED;
        config_destroy(config_a_escribir);
        free(metadata_config_asociado);
    }
    else
    {
        char** bloques_actuales = config_get_array_value(config_a_escribir, "Blocks");

        int cantidad_bloques = 0;
        while(bloques_actuales[cantidad_bloques] != NULL){
            cantidad_bloques++;
        }


        if (num_bloque_Log >= cantidad_bloques) {
            log_error(logger, "<%d> - Error: Intento de escritura fuera del tamaño del archivo (Truncate insuficiente). Indice solicitado: %d, Bloques disponibles: %d", query_id, num_bloque_Log, cantidad_bloques);
            *error = ERR_LECTURA_FUERA_DEL_LIMITE;
            string_iterate_lines(bloques_actuales, (void*) free);
            free(bloques_actuales);
            config_destroy(config_a_escribir);
            free(metadata_config_asociado);
            return;
        }

        int bloque_fisico_actual = atoi(bloques_actuales[num_bloque_Log]);

        char* path_bloque_fisico = malloc(strlen(dir_physical_blocks) + 20);
        char* nombre_bloque_fisico = malloc(20);

        sprintf(path_bloque_fisico, "%s/block%04d.dat", dir_physical_blocks, bloque_fisico_actual);
        sprintf(nombre_bloque_fisico, "/block%04d%s", bloque_fisico_actual, "\0");

        struct stat st;
        int bloque_final_escrito = bloque_fisico_actual;

        if (stat(path_bloque_fisico, &st) == 0) {
            
            if (st.st_nlink > 1 || !bloque_fisico_actual) {
                int nuevo_bloque_libre = espacio_disponible(bitmap);
                
                if (nuevo_bloque_libre == -1) {
                    log_error(logger, "Error, Espacio Insuficiente para Copy-On-Write");
                    *error = ERR_ESPACIO_INSUFICIENTE;
                } 
                else {
                    buscar_y_ocupar_siguiente_bit_libre(nuevo_bloque_libre);
                    bloque_final_escrito = nuevo_bloque_libre;

                    char* nombre_nuevo_bloque = malloc(20);
                    sprintf(nombre_nuevo_bloque, "/block%04d", nuevo_bloque_libre);
                    
                    char* path_nuevo_bloque_fisico = malloc(strlen(dir_physical_blocks) + 20);
                    sprintf(path_nuevo_bloque_fisico, "%s%s.dat", dir_physical_blocks, nombre_nuevo_bloque);

                    FILE* f_bloque = fopen(path_nuevo_bloque_fisico, "w");
                    if (f_bloque != NULL) {
                        fwrite(contenido, sizeof(char), tam_bloque, f_bloque);
                        fclose(f_bloque);
                    }
                    char * hash_nuevo = crypto_md5(contenido, tam_bloque);
                    if(strcmp(retornar_hash(nombre_bloque_fisico),hash_nuevo)){
                        escribir_en_hash(nombre_nuevo_bloque);
                    }
                    

                    char* archivo_tag = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 5);
                    sprintf(archivo_tag, "%s/%s/%s", dir_files, archivo, tag);
                    
                    char* logical_block_path = malloc(strlen(archivo_tag) + 50);
                    sprintf(logical_block_path, "%s/logical_blocks/%06d.dat", archivo_tag, num_bloque_Log);

                    unlink(logical_block_path);
                    link(path_nuevo_bloque_fisico, logical_block_path);

                    free(bloques_actuales[num_bloque_Log]);
                    bloques_actuales[num_bloque_Log] = string_itoa(nuevo_bloque_libre);

                    char* lista_final = string_new();
                    string_append(&lista_final, "[");
                    for (int i = 0; bloques_actuales[i] != NULL; i++) {
                        if (i > 0) string_append(&lista_final, ",");
                        string_append(&lista_final, bloques_actuales[i]);
                    }
                    string_append(&lista_final, "]");
                    
                    config_set_value(config_a_escribir, "Blocks", lista_final);
                    config_save(config_a_escribir);

                    free(lista_final);
                    free(nombre_nuevo_bloque);
                    free(path_nuevo_bloque_fisico);
                    free(archivo_tag);
                    free(logical_block_path);
                }
            } 
            else {
                FILE* f_bloque = fopen(path_bloque_fisico, "w");
                if (f_bloque != NULL) {
                    fwrite(contenido, sizeof(char), strlen(contenido), f_bloque);
                    fclose(f_bloque);
                }
            }
        } else {
             log_error(logger, "Error: No se encontró el archivo físico %s", path_bloque_fisico);
             *error = ERR_FILE_INEXISTENTE;
        }

        log_info(logger,"<%d> - Bloque Lógico Escrito <%s>:<%s> - Número de Bloque Físico: <%d>", query_id, archivo, tag, bloque_final_escrito);

        string_iterate_lines(bloques_actuales, (void*) free);
        free(bloques_actuales);
        free(path_bloque_fisico);
        config_destroy(config_a_escribir);
        free(metadata_config_asociado);
        
    }
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

    char *path_completo = string_from_format("%s%s.dat", dir_physical_blocks, nombre_bloque);
    FILE *f_bloque = fopen(path_completo, "rb");
    
    if (f_bloque == NULL) {
        log_error(logger, "No se pudo abrir el bloque en: %s", path_completo);
        config_destroy(hash_config);
        free(path_completo);
        return NULL;
    }

    fseek(f_bloque, 0, SEEK_END);
    long fsize = ftell(f_bloque);
    fseek(f_bloque, 0, SEEK_SET);

    char *buffer_contenido = malloc(fsize + 1);
    fread(buffer_contenido, 1, fsize, f_bloque);
    buffer_contenido[fsize] = '\0';
    fclose(f_bloque);

    char *nombre_bloque_hash = crypto_md5(buffer_contenido, fsize); 
    char *nombre_bloque_sin_prefijo = nombre_bloque + 1; 
    config_set_value(hash_config, nombre_bloque_hash, nombre_bloque_sin_prefijo);

    if (config_save(hash_config) == -1) {
        log_error(logger, "Error al guardar los datos en blocks_hash_index.config");
    }

    config_destroy(hash_config);
    free(path_completo);
    free(buffer_contenido);

    return nombre_bloque_hash; 
}

char* retornar_hash(char *nombre_bloque) {
    int path_len = strlen(dir_physical_blocks) + strlen(nombre_bloque) + 6;
    char* path_completo = malloc(path_len);
    sprintf(path_completo, "%s/%s.dat", dir_physical_blocks, nombre_bloque);

    FILE *f_bloque = fopen(path_completo, "rb");

    if (f_bloque == NULL) {
        log_error(logger, "No se pudo abrir el bloque en: %s", path_completo);
        free(path_completo);
        return NULL;
    }

    fseek(f_bloque, 0, SEEK_END);
    long fsize = ftell(f_bloque);
    fseek(f_bloque, 0, SEEK_SET);

    char *buffer_contenido;

    if (fsize == 0) {
        buffer_contenido = malloc(1);
        buffer_contenido[0] = '\0';
    } else {
        buffer_contenido = malloc(fsize + 1);
        fread(buffer_contenido, 1, fsize, f_bloque);
        buffer_contenido[fsize] = '\0';
    }

    fclose(f_bloque);

    char *nombre_bloque_hash = crypto_md5(buffer_contenido, fsize);

    free(path_completo);
    free(buffer_contenido);

    return nombre_bloque_hash;
}



char* Leer_bloque(char* archivo, char* tag, int num_bloque_Log, int query_id, protocolo_socket * error){
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config", dir_files, archivo, tag);

    t_config *config_a_leer = config_create(metadata_config_asociado);

    if (config_a_leer == NULL) {
        log_error(logger, "Error: No se pudo abrir la metadata en %s", metadata_config_asociado);
        *error = ERR_METADATA;
        free(metadata_config_asociado);
        return NULL;
    }

    char** bloques_actuales = config_get_array_value(config_a_leer, "Blocks");

    int cantidad_bloques = 0;
    while(bloques_actuales[cantidad_bloques] != NULL){
        cantidad_bloques++;
    }

    if (num_bloque_Log >= cantidad_bloques) {
        log_error(logger, "<%d> - Error: Intento de lectura fuera de rango. Indice: %d, Bloques disponibles: %d", query_id, num_bloque_Log, cantidad_bloques);
        *error = ERR_LECTURA_FUERA_DEL_LIMITE;
        string_iterate_lines(bloques_actuales, (void*) free);
        free(bloques_actuales);
        config_destroy(config_a_leer);
        free(metadata_config_asociado);
        return NULL;
    }

    int bloque_fisico_actual = atoi(bloques_actuales[num_bloque_Log]);

    char* path_bloque_fisico = malloc(strlen(dir_physical_blocks) + 20);
    sprintf(path_bloque_fisico, "%s/block%04d.dat", dir_physical_blocks, bloque_fisico_actual);

    FILE* f_bloque = fopen(path_bloque_fisico, "r");
    char* buffer_contenido = calloc(tam_bloque, 1);

    if (f_bloque != NULL) {
        fseek(f_bloque, 0, SEEK_END);
        long fsize = ftell(f_bloque);
        fseek(f_bloque, 0, SEEK_SET);

        fread(buffer_contenido, 1, fsize, f_bloque);
        buffer_contenido[fsize] = '\0';
        
        fclose(f_bloque);
        log_info(logger,"<%d> - Bloque Lógico Leído <%s>:<%s> - Número de Bloque Físico: <%d>", query_id, archivo, tag, bloque_fisico_actual);
        log_debug(logger,"Data: leida <%s>", buffer_contenido);
    } else {
        log_error(logger, "Error: No se pudo abrir el archivo físico %s", path_bloque_fisico);
        *error = ERR_ESCRITURA_BLOQUE;
    }

    string_iterate_lines(bloques_actuales, (void*) free);
    free(bloques_actuales);
    free(path_bloque_fisico);
    config_destroy(config_a_leer);
    free(metadata_config_asociado);

    return buffer_contenido;
}

void Crear_tag(char * Origen, char * Destino, char* tag_origen, char* tag_destino, int query_id, protocolo_socket * error) {

    char* ruta_origen = malloc(strlen(dir_files) + strlen(Origen) + strlen(tag_origen) + 3);
    sprintf(ruta_origen, "%s/%s/%s", dir_files, Origen, tag_origen);

    char* ruta_destino = malloc(strlen(dir_files) + strlen(Destino) + strlen(tag_destino) + 3);
    sprintf(ruta_destino, "%s/%s/%s", dir_files, Destino, tag_destino);

    DIR* dir = opendir(ruta_destino);
    if (dir) {
        closedir(dir);
        log_error(logger, "Error, Tag preexistente");
        *error = ERR_TAG_PREEXISTENTE;
        free(ruta_origen);
        free(ruta_destino);
        return;
    }

    char* comando = malloc(strlen("cp -r ") + strlen(ruta_origen) + strlen(" ") + strlen(ruta_destino) + 1);
    sprintf(comando, "cp -r %s %s", ruta_origen, ruta_destino);

    system(comando);

    free(ruta_origen);
    free(ruta_destino);
    free(comando);

    char* ruta_archivo = malloc(strlen(dir_files) + strlen(Destino) + strlen(tag_destino) + strlen("/metadata.config") + 3);
    sprintf(ruta_archivo, "%s/%s/%s/metadata.config", dir_files, Destino, tag_destino);
    
    t_config *config_archivo = config_create(ruta_archivo);

    if (config_archivo == NULL) {
        log_error(logger, "No se pudo abrir el config");
        *error = ERR_METADATA;
        free(ruta_archivo);
        return;
    }

    config_set_value(config_archivo, "ESTADO", "WORK IN PROGRESS");
    config_save(config_archivo);
    config_destroy(config_archivo);
    free(ruta_archivo);
    log_info(logger,"<%d> - Tag Creado <%s>:<%s>", query_id, Destino, tag_destino);
};

void Eliminar_tag(char * Origen, char* tag, int query_id, protocolo_socket * error){

    if (strcmp(Origen, "initial_file") == 0 && strcmp(tag, "BASE") == 0) {
        log_error(logger, "Error: NO SE PUEDE ELIMINAR EL TAG initial_file/BASE");
        *error = ERR_FORBIDDEN;
    } else {
        int length = strlen(dir_files) + strlen(Origen) + strlen(tag) + 15;
        char * comando = malloc(length);

        if (comando == NULL) return;

        sprintf(comando, "rm -rf %s/%s/%s", dir_files, Origen, tag);

        int resultado = system(comando);

        if (resultado == 0) {
            log_info(logger,"<%d> - Tag Eliminado <%s>:<%s>", query_id, Origen, tag);
        } else {
            log_error(logger, "Error al eliminar el tag");
            *error = ERR_TAG_INEXISTENTE;
        }

        free(comando);
    }
}

void Commit_tag(char* archivo, char* tag, int query_id, protocolo_socket * error) {
    char* ruta_metadata = string_from_format("%s/%s/%s/metadata.config", dir_files, archivo, tag);
    t_config *config_archivo = config_create(ruta_metadata);

    if (config_archivo == NULL) {
        log_error(logger, "No se pudo abrir el config en: %s", ruta_metadata);
        *error = ERR_METADATA;
        free(ruta_metadata);
        return;
    }

    if (config_has_property(config_archivo, "ESTADO")) {
        char *estado_actual = config_get_string_value(config_archivo, "ESTADO");
        if (strcmp(estado_actual, "COMMITED") == 0) {
            log_info(logger, "<%d> - El Tag <%s>:<%s> ya está COMMITED. Fin.", query_id, archivo, tag);
            config_destroy(config_archivo);
            free(ruta_metadata);
            return;
        }
    }

    t_config *hash_config = config_create(path_hash);
    if (!hash_config) {
        FILE *f = fopen(path_hash, "w");
        if(f) fclose(f);
        hash_config = config_create(path_hash);
    }

    char **bloques = config_get_array_value(config_archivo, "Blocks");
    bool hubo_cambios = false;

    for (int i = 0; bloques[i] != NULL; i++) {
        
        if (strcmp(bloques[i], "0") == 0) continue;

        char *id_bloque_actual_str = bloques[i];
        int id_bloque_actual_int = atoi(id_bloque_actual_str);
        char *nombre_fisico_actual = string_from_format("block%04d", id_bloque_actual_int);

        char *hash_calculado = retornar_hash(nombre_fisico_actual); 

        if (config_has_property(hash_config, hash_calculado)) {
            
            char *nombre_fisico_preexistente = config_get_string_value(hash_config, hash_calculado);

            if (strcmp(nombre_fisico_actual, nombre_fisico_preexistente) != 0) {
                
                char *path_fisico_actual = string_from_format("%s/%s.dat", dir_physical_blocks, nombre_fisico_actual);
                char *path_fisico_preexistente = string_from_format("%s/%s.dat", dir_physical_blocks, nombre_fisico_preexistente);
                
                char *path_logico = string_from_format("%s/%s/%s/logical_blocks/%06d.dat", dir_files, archivo, tag, i);

                log_info(logger, "<%d> - <%s>:<%s> Se eliminó el hard link del bloque lógico <%d> al bloque físico <%s>",
                        query_id, archivo, tag, i, nombre_fisico_actual);

                unlink(path_logico);
                
                if (link(path_fisico_preexistente, path_logico) < 0) {
                    log_error(logger, "Error linkeando %s a %s", path_fisico_preexistente, path_logico);
                }

                liberar_espacio_bitmap(id_bloque_actual_int);

                log_info(logger, "<%d> - <%s>:<%s> Bloque Lógico <%d> se reasigna de <%s> a <%s>",
                         query_id, archivo, tag, i, nombre_fisico_actual, nombre_fisico_preexistente);


                int id_nuevo_int;
                sscanf(nombre_fisico_preexistente, "block%d", &id_nuevo_int);
                
                free(bloques[i]);
                bloques[i] = string_itoa(id_nuevo_int);
                
                hubo_cambios = true;

                free(path_fisico_actual);
                free(path_fisico_preexistente);
                free(path_logico);
            }
        } else {
            config_set_value(hash_config, hash_calculado, nombre_fisico_actual);
            config_save(hash_config);
        }

        free(nombre_fisico_actual);
        free(hash_calculado);
    }

    if (hubo_cambios) {
        char *lista_bloques_str = string_array_to_string(bloques);
        config_set_value(config_archivo, "Blocks", lista_bloques_str);
        free(lista_bloques_str);
    }

    config_set_value(config_archivo, "ESTADO", "COMMITED");
    config_save(config_archivo);
    log_info(logger, "<%d> - Commit de File:Tag %s:%s",query_id,archivo, tag);

    string_array_destroy(bloques);
    config_destroy(hash_config);
    config_destroy(config_archivo);
    free(ruta_metadata);
}


void esperar(int milisegundos){
    usleep(milisegundos*1000);
}

