#include <worker_query_interpreter.h>


int block_size = 0;
int mem_delay_ms = 0;
extern int socket_storage;
extern int socket_master;
extern bool flag_COMMIT == false;

int program_counter;
int query_id;
char * query_path;

extern pthread_mutex_t * mutex_flag_desalojo;

static t_instruccion obtener_instruccion(const char* texto) {
    if (strcmp(texto, "CREATE")   == 0) return CREATE;
    if (strcmp(texto, "TRUNCATE") == 0) return TRUNCATE;
    if (strcmp(texto, "WRITE")    == 0) return WRITE;
    if (strcmp(texto, "READ")     == 0) return READ;
    if (strcmp(texto, "TAG")      == 0) return TAG;
    if (strcmp(texto, "COMMIT")   == 0) return COMMIT;
    if (strcmp(texto, "FLUSH")    == 0) return FLUSH;
    if (strcmp(texto, "DELETE")   == 0) return DELETE;
    if (strcmp(texto, "END")      == 0) return END;
    return INVALID_INSTRUCTION;
}


static inline void quitar_salto_de_linea(char* cadena) {
    if (!cadena) 
        return;
    int L = (int)strlen(cadena);
    if (L > 0 && cadena[L-1] == '\n') 
        cadena[L-1] = '\0';
}

static void liberar_string_split(char** array) {
    for (int i = 0; array[i] != NULL; i++)
        free(array[i]);
    free(array);
}

qi_status_t obtener_instruccion_y_args(void* parametros_worker, const char* linea, int query_id) {
    if (!linea || strlen(linea) == 0)
        return QI_OK; //osea, si viene una linea vacia, que pase de largo. Tipo, ta bien

    char** partes = string_split(linea, " ");
    if (!partes || !partes[0]) {
        liberar_string_split(partes);
        return QI_ERR_PARSE;
    }

    t_instruccion instr = obtener_instruccion(partes[0]);
    qi_status_t status = interpretar_Instruccion(instr, partes, query_id);

    liberar_string_split(partes);
    return status;
}

static bool separar_nombre_y_tag(const char* cadena, char** nombre_out, char** tag_out) {
    if (!cadena || !nombre_out || !tag_out) 
        return false;
    // formato esperado: <NOMBRE_FILE>:<TAG>
    const char* p = strchr(cadena, ':');
    if (!p) 
        return false;

    int long_nombre = (int)(p - cadena);
    int long_tag = strlen(p + 1);

    if (long_nombre == 0 || long_tag == 0) 
        return false;

    char* nombre = malloc(long_nombre + 1);
    char* tag    = malloc(long_tag + 1);

    if (!nombre || !tag) { 
        free(nombre); 
        free(tag); 
        return false; 
    }

    memcpy(nombre, cadena, long_nombre);
    nombre[long_nombre] = '\0';
    memcpy(tag, p + 1, long_tag + 1);

    *nombre_out = nombre;
    *tag_out    = tag;
    return true;
}


qi_status_t interpretar_Instruccion(t_instruccion instruccion, char** args, int query_id) {
    char *archivo = NULL, *tag = NULL;
    qi_status_t result = QI_OK;

    switch (instruccion) {
        case CREATE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            if (!separar_nombre_y_tag(args[1], &archivo, &tag)) 
                return QI_ERR_PARSE;
            result = ejecutar_CREATE(archivo, tag, query_id);
            break;

        case TRUNCATE:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_TRUNCATE(archivo, tag, atoi(args[2]), query_id);
            if (result == QI_OK)
                memoria_invalidar_paginas_fuera_de_rango(archivo, tag, atoi(args[2]));
            break;

        case WRITE:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_WRITE(archivo, tag, atoi(args[2]), args[3], query_id);
            break;

        case READ:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_READ(archivo, tag, atoi(args[2]), atoi(args[3]), query_id);
            break;

        case TAG:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            char *arch_ori, *tag_ori, *arch_dest, *tag_dest;
            separar_nombre_y_tag(args[1], &arch_ori, &tag_ori);
            separar_nombre_y_tag(args[2], &arch_dest, &tag_dest);
            result = ejecutar_TAG(arch_ori, tag_ori, arch_dest, tag_dest, query_id);
            free(arch_ori); 
            free(tag_ori); 
            free(arch_dest); 
            free(tag_dest);
            return result;

        case COMMIT:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_COMMIT(archivo, tag, query_id);
            break;

        case FLUSH:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_FLUSH(archivo, tag, query_id);
            break;

        case DELETE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_DELETE(archivo, tag, query_id);
            break;

        case END:
             log_info(logger, "## Query %d: Finalizando query", query_id);
             memoria_flush_global(query_id);
            return QI_END;

        default:
            log_error(logger, "Instrucción desconocida: %s", args[0]);
            result = QI_ERR_PARSE;
            break;
    }

    free(archivo);
    free(tag);
    return result;
}
int obtener_desalojo_flag(){
    pthread_mutex_lock(mutex_flag_desalojo);
    int return_value = flag_desalojo;
    pthread_mutex_unlock(mutex_flag_desalojo);

    return return_value;
}

void setear_desalojo_flag(int value){
    pthread_mutex_lock(mutex_flag_desalojo);
    flag_desalojo = value;
    pthread_mutex_unlock(mutex_flag_desalojo);
}

void loop_principal(){
    protocolo_socket cod_op;
    int flag_fin_query = false;

    mutex_flag_desalojo = inicializarMutex();

    //hay que modificar ejecutar query para que corra de a una linea, dandonos la oporturnidad de chequear si hay desalojo desde master
    while(1){
        if (obtener_desalojo_flag()){
            memoria_flush_global(query_id);
            //llamo a una funcion que atienda el desalojo, o escribir aca mismo. Hay que darle el PC a master
            //seteo el flag en false
            setear_desalojo_flag(false);
            break;
            //sigo con mi vida
        }if(flag_fin_query){
            break;
        }
        ejecutar_query(query_path, query_id);
    }

   
}

void ejecutar_query(const char* path_query, int query_id) {
    FILE* arch_inst = fopen(path_query, "r");
    if (!arch_inst) {
        log_error(logger, "No se pudo abrir el archivo de Query: %s", path_query);
        return;
    }

    log_info(logger, "## Query %d: Se recibe la Query. El path de operaciones es: %s", query_id, path_query);

    char* linea = NULL;
    size_t len = 0;
    size_t read;

    //este while no deberia esta, asi solo lee una linea. El file read se puede hacer en otro lado (loop_principal()? y recorrer las lineas en un char* usando el program counter)
    while ((read = getline(&linea, &len, arch_inst)) != -1) {
        quitar_salto_de_linea(linea);

        log_info(logger, "## Query %d: FETCH - Program Counter: %d - %s", query_id, program_counter, linea);

        qi_status_t st = obtener_instruccion_y_args(NULL, linea, query_id);

        log_info(logger, "## Query %d: - Instrucción realizada: %s", query_id, linea);

        if (st == QI_END) {
            log_info(logger, "## Query %d: Query finalizada correctamente", query_id);
            break;
        }

        if (st == QI_ERR_PARSE) {
            log_error(logger, "## Query %d: Error de parseo en línea %d", query_id, program_counter);
            break;
        }

        program_counter++;
    }

    free(linea);
    fclose(arch_inst);
}

qi_status_t ejecutar_CREATE(char* archivo, char* tag, int query_id) {
    log_info(logger, "## Query %d: Ejecutando CREATE %s:%s", query_id, archivo, tag);

    t_paquete* paquete = crear_paquete(OP_CREATE);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &(query_id), sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    log_info(logger, "## Query %d: Enviado CREATE -> Storage (%s:%s)", query_id, archivo, tag);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: CREATE exitoso", query_id);
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en CREATE", query_id);
        return QI_ERR_PARSE;
    }
}

qi_status_t ejecutar_TRUNCATE(char* archivo, char* tag, int tamanio, int query_id) {
    log_info(logger, "## Query %d: Ejecutando TRUNCATE %s:%s %d", query_id, archivo, tag, tamanio);

    
    t_paquete* paquete = crear_paquete(OP_TRUNCATE);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, &(query_id), sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: TRUNCATE exitoso", query_id);
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en TRUNCATE", query_id);
        return QI_ERR_PARSE;
    }
}

qi_status_t ejecutar_WRITE(char* archivo, char* tag, int dir_base, char* contenido, int query_id)
{
    log_info(logger,"## Query %d: Ejecutando WRITE %s:%s %d '%s'",query_id, archivo, tag, dir_base, contenido);


    qi_status_t status = ejecutar_WRITE_memoria(archivo, tag, dir_base, contenido, query_id);

    if (status == QI_OK) {
        log_info(logger, "## Query %d: WRITE exitoso en memoria interna", query_id);
    } else {
        log_error(logger, "## Query %d: Error en WRITE (memoria interna)", query_id);
    }

    return status;
}

qi_status_t ejecutar_READ(char* archivo, char* tag, int dir_base, int tamanio, int query_id)
{
    log_info(logger,
            "## Query %d: Ejecutando READ %s:%s %d %d",query_id, archivo, tag, dir_base, tamanio);

    qi_status_t status = ejecutar_READ_memoria(archivo, tag, dir_base, tamanio, query_id);

    if (status == QI_OK) {
        log_info(logger, "## Query %d: READ exitoso", query_id);
    } else {
        log_error(logger, "## Query %d: Error en READ (memoria interna)", query_id);
    }

    return status;
}

qi_status_t ejecutar_TAG(char* arch_ori, char* tag_ori, char* arch_dest, char* tag_dest, int query_id) {
    memoria_actualizar_tag(arch_ori, tag_ori, arch_dest, tag_dest);
    log_info(logger, "## Query %d: Ejecutando TAG %s:%s -> %s:%s", query_id, arch_ori, tag_ori, arch_dest, tag_dest);

    t_paquete* paquete = crear_paquete(OP_TAG);
    agregar_a_paquete(paquete, arch_ori, strlen(arch_ori) + 1);
    agregar_a_paquete(paquete, tag_ori, strlen(tag_ori) + 1);
    agregar_a_paquete(paquete, arch_dest, strlen(arch_dest) + 1);
    agregar_a_paquete(paquete, tag_dest, strlen(tag_dest) + 1);
    agregar_a_paquete(paquete, &(query_id), sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: TAG exitoso", query_id);
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en TAG", query_id);
        return QI_ERR_PARSE;
    }
}

qi_status_t ejecutar_COMMIT(char* archivo, char* tag, int query_id) {
    log_info(logger, "## Query %d: Ejecutando COMMIT %s:%s",query_id, archivo, tag);

    qi_status_t st = ejecutar_FLUSH_memoria(archivo, tag, query_id);
    if (st != QI_OK) {
        log_error(logger, "## Query %d: Error durante FLUSH previo al COMMIT",query_id);
        return st;
    }

    t_paquete* paquete = crear_paquete(OP_COMMIT);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &query_id, sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    flag_COMMIT == true;
    
    protocolo_socket resp = recibir_paquete_ok(socket_storage);
    if (resp == OK) {
        log_info(logger, "## Query %d: COMMIT exitoso", query_id);
        return QI_OK;
    }

    log_error(logger, "## Query %d: Error en COMMIT", query_id);
    return QI_ERR_PARSE;
}


qi_status_t ejecutar_FLUSH(char* archivo, char* tag, int query_id) {
    log_info(logger, "## Query %d: Ejecutando FLUSH %s:%s",query_id, archivo, tag);

    qi_status_t st = ejecutar_FLUSH_memoria(archivo, tag, query_id);

    if (st == QI_OK)
        log_info(logger, "## Query %d: FLUSH exitoso", query_id);
    else
        log_error(logger, "## Query %d: Error en FLUSH", query_id);

    return st;
}
qi_status_t ejecutar_DELETE(char* archivo, char* tag, int query_id) {
    log_info(logger, "## Query %d: Ejecutando DELETE %s:%s", query_id, archivo, tag);

    t_paquete* paquete = crear_paquete(OP_DELETE);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &(query_id), sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: DELETE exitoso", query_id);
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en DELETE", query_id);
        return QI_ERR_PARSE;
    }
}