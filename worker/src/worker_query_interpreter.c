#include <worker_query_interpreter.h>


int block_size = 0;
int mem_delay_ms = 0;
extern int socket_master;
extern sem_t * sem_desalojo_waiter;

extern char * ip_storage, *puerto_storage;
extern int socket_master;


qi_status_t ejecutar_WRITE_memoria(char * archivo,char * tag,int dir_base,char * contenido);
qi_status_t ejecutar_READ_memoria(char* archivo, char* tag, int direccion_base, int tamanio);
qi_status_t ejecutar_FLUSH_memoria(char* archivo, char* tag);

void inicializar_memoria_interna(int tam_total, int tam_pagina);
void inicializar_paginas();

void memoria_agregar_commit(const char* archivo, const char* tag);
void liberar_memoria_interna();
void memoria_invalidar_paginas_fuera_de_rango(const char* archivo, const char* tag, int nuevo_tamanio);

int seleccionar_victima();


void proxima_victima(char * tag);
void ocuapar_espacio(int victima,char * tag);

void memoria_actualizar_tag( char* arch_o,  char* tag_o, char* arch_n,  char* tag_n);
void memoria_invalidar_file_tag_completo(const char* archivo, const char* tag);
void memoria_eliminar_commit(const char* archivo, const char* tag);
void memoria_truncar(const char* archivo, const char* tag, int nuevo_tam);

qi_status_t memoria_flush_global();
bool hubo_COMMIT_no_se_puede_WRITE(const char* archivo, const char* tag);

int obtener_query_id();

int program_counter;
char * query_path;

extern pthread_mutex_t * mutex_flag_desalojo;
extern sem_t * sem_hay_query;

extern list_struct_t * lista_queries;


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

qi_status_t obtener_instruccion_y_args(void* parametros_worker,  char* linea) {
    if (!linea || strlen(linea) == 0)
        return QI_OK; //osea, si viene una linea vacia, que pase de largo. Tipo, ta bien

    char** partes = string_split(linea, " ");
    if (!partes || !partes[0]) {
        liberar_string_split(partes);
        return QI_ERR_PARSE;
    }

    t_instruccion instr = obtener_instruccion(partes[0]);
    qi_status_t status = interpretar_Instruccion(instr, partes);

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


qi_status_t interpretar_Instruccion(t_instruccion instruccion, char** args) {
    char *archivo = NULL, *tag = NULL;
    qi_status_t result = QI_OK;

    switch (instruccion) {
        case CREATE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            if (!separar_nombre_y_tag(args[1], &archivo, &tag)) 
                return QI_ERR_PARSE;
            result = ejecutar_CREATE(archivo, tag);
            break;

        case TRUNCATE:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_TRUNCATE(archivo, tag, atoi(args[2]));
            if (result == QI_OK)
                memoria_invalidar_paginas_fuera_de_rango(archivo, tag, atoi(args[2]));
            break;

        case WRITE:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_WRITE(archivo, tag, atoi(args[2]), args[3]);
            break;

        case READ:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_READ(archivo, tag, atoi(args[2]), atoi(args[3]));
            break;

        case TAG:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            char *arch_ori, *tag_ori, *arch_dest, *tag_dest;
            separar_nombre_y_tag(args[1], &arch_ori, &tag_ori);
            separar_nombre_y_tag(args[2], &arch_dest, &tag_dest);
            result = ejecutar_TAG(arch_ori, tag_ori, arch_dest, tag_dest);
            free(arch_ori); 
            free(tag_ori); 
            free(arch_dest); 
            free(tag_dest);
            return result;

        case COMMIT:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_COMMIT(archivo, tag);
            break;

        case FLUSH:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_FLUSH(archivo, tag);
            break;

        case DELETE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_DELETE(archivo, tag);
            break;

        case END:
             log_info(logger, "## Query %d: Finalizando query", obtener_query_id());
             memoria_flush_global();
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
        sem_wait(sem_hay_query);
        if (obtener_desalojo_flag()){

            memoria_flush_global();
            sem_wait(sem_desalojo_waiter);

            setear_desalojo_flag(false);

        }else {
            if (!ejecutar_query(query_path)){
                sem_post(sem_hay_query);
            }else {
                enviar_error_a_master(WORKER_FINALIZACION,"Query END");
            }
        }  
    }

   
}

char * obtener_instruccion_index(list_struct_t * lista_queries, int PC){
    pthread_mutex_lock(lista_queries->mutex);

    char * returnvalue = list_get(lista_queries->lista, PC);

    pthread_mutex_unlock(lista_queries->mutex);

    return returnvalue;
}

int ejecutar_query(const char* path_query) {

    char* linea = obtener_instruccion_index(lista_queries, program_counter);
    size_t len = 0;
    size_t read;

    //este while no deberia esta, asi solo lee una linea. El file read se puede hacer en otro lado (loop_principal()? y recorrer las lineas en un char* usando el program counter)

    quitar_salto_de_linea(linea);

    log_info(logger, "## Query %d: FETCH - Program Counter: %d - %s", obtener_query_id(), program_counter, linea);

    qi_status_t st = obtener_instruccion_y_args(NULL, linea);

    log_info(logger, "## Query %d: - Instrucción realizada: %s", obtener_query_id(), linea);

    if (st == QI_END) {
        log_info(logger, "## Query %d: Query finalizada correctamente", obtener_query_id());        
        return 1;
    }

    if (st == QI_ERR_PARSE) {
        log_error(logger, "## Query %d: Error de parseo en línea %d", obtener_query_id(), program_counter);
        
    }

    program_counter++;

    return 0;

    free(linea);
}

qi_status_t ejecutar_CREATE(char* archivo, char* tag) {
    log_info(logger, "## Query %d: Ejecutando CREATE %s:%s", obtener_query_id(), archivo, tag);

    int query_id_temp = obtener_query_id(); 
    
    t_paquete* paquete = crear_paquete(OP_CREATE);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    log_info(logger, "## Query %d: Enviado CREATE -> Storage (%s:%s)", obtener_query_id(), archivo, tag);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    close(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: CREATE exitoso", obtener_query_id());
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en CREATE", obtener_query_id());
        enviar_error_a_master(WORKER_FINALIZACION,"Error en CREATE");
        return QI_ERR_STORAGE;
    }
}

qi_status_t ejecutar_TRUNCATE(char* archivo, char* tag, int nuevo_tamanio) {
log_info(logger, "## Query %d: Ejecutando TRUNCATE %s:%s %d", obtener_query_id(), archivo, tag, nuevo_tamanio);

    // Validar COMMIT
    if (hubo_COMMIT_no_se_puede_WRITE(archivo, tag)) {
        log_error(logger,"## Query %d: TRUNCATE prohibido: %s:%s tiene COMMIT",obtener_query_id(), archivo, tag);
        enviar_error_a_master(WORKER_FINALIZACION,"TRUNCATE prohibido porque tiene COMMIT");
        return QI_ERR_COMMIT_CERRADO;
    }

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_TRUNCATE);
    agregar_a_paquete(paquete, archivo, strlen(archivo)+1);
    agregar_a_paquete(paquete, tag, strlen(tag)+1);
    agregar_a_paquete(paquete, &nuevo_tamanio, sizeof(int));
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    protocolo_socket resp = recibir_paquete_ok(socket_storage);
    close(socket_storage);
    if (resp != OK) {
        log_error(logger,"## Query %d: Storage rechazó TRUNCATE %s:%s",obtener_query_id(), archivo, tag);
        enviar_error_a_master(WORKER_FINALIZACION,"Storage rechazó TRUNCATE");
        return QI_ERR_STORAGE;
    }

    // TRUNCATE aceptado -> actualizar memoria
    memoria_truncar(archivo, tag, nuevo_tamanio);

    log_info(logger,"## Query %d: TRUNCATE %s:%s exitoso",obtener_query_id(), archivo, tag);

    return QI_OK;
}

qi_status_t ejecutar_WRITE(char* archivo, char* tag, int dir_base, char* contenido)
{
    log_info(logger,"## Query %d: Ejecutando WRITE %s:%s %d '%s'",obtener_query_id(), archivo, tag, dir_base, contenido);


    qi_status_t status = ejecutar_WRITE_memoria(archivo, tag, dir_base, contenido);

    if (status == QI_OK) {
        log_info(logger, "## Query %d: WRITE exitoso en memoria interna", obtener_query_id());
    } else {
        log_error(logger, "## Query %d: Error en WRITE (memoria interna)", obtener_query_id());
        enviar_error_a_master(WORKER_FINALIZACION,"Error en WRITE ");
        return QI_ERR_PARSE;
    }

    return status;
}

qi_status_t ejecutar_READ(char* archivo, char* tag, int dir_base, int tamanio)
{
    log_info(logger,
            "## Query %d: Ejecutando READ %s:%s %d %d",obtener_query_id(), archivo, tag, dir_base, tamanio);

    qi_status_t status = ejecutar_READ_memoria(archivo, tag, dir_base, tamanio);

    if (status == QI_OK) {
        log_info(logger, "## Query %d: READ exitoso", obtener_query_id());
    } else {
        log_error(logger, "## Query %d: Error en READ (memoria interna)", obtener_query_id());
        enviar_error_a_master(WORKER_FINALIZACION,"Error en READ");
        return QI_ERR_PARSE;
    }

    return status;
}

qi_status_t ejecutar_TAG(char* arch_ori, char* tag_ori, char* arch_dest, char* tag_dest) {
    //  Validar COMMIT destino
    if (hubo_COMMIT_no_se_puede_WRITE(arch_dest, tag_dest)) {
        log_error(logger,"## Query %d: TAG no permitido: destino %s:%s tiene COMMIT", obtener_query_id(), arch_dest, tag_dest);
        enviar_error_a_master(WORKER_FINALIZACION,"TAG no permitido");
        return QI_ERR_COMMIT_CERRADO;
    }

    // Si origen tiene commit, marcar commit en destino (a nivel lógico)
    bool origen_comiteado = hubo_COMMIT_no_se_puede_WRITE(arch_ori, tag_ori);

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_TAG);
    agregar_a_paquete(paquete, arch_ori, strlen(arch_ori)+1);
    agregar_a_paquete(paquete, tag_ori,  strlen(tag_ori)+1);
    agregar_a_paquete(paquete, arch_dest, strlen(arch_dest)+1);
    agregar_a_paquete(paquete, tag_dest,  strlen(tag_dest)+1);
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    protocolo_socket r = recibir_paquete_ok(socket_storage);
    close(socket_storage);

    if (r != OK) {
        log_error(logger, "## Query %d: Storage rechazó TAG", obtener_query_id());
        enviar_error_a_master(WORKER_FINALIZACION,"Storage rechazó TAG");
        return QI_ERR_STORAGE;
    }

    memoria_actualizar_tag(arch_ori, tag_ori, arch_dest, tag_dest);

    // Mover commit origen -> destino
    if (origen_comiteado) {
        memoria_eliminar_commit(arch_ori, tag_ori);
        memoria_agregar_commit(arch_dest, tag_dest);
    }

    log_info(logger, "## Query %d: TAG exitoso", obtener_query_id());
    return QI_OK;
}

qi_status_t ejecutar_COMMIT(char* archivo, char* tag) {
    log_info(logger, "## Query %d: Ejecutando COMMIT %s:%s",obtener_query_id(), archivo, tag);

    qi_status_t st = ejecutar_FLUSH_memoria(archivo, tag);
    if (st != QI_OK) {
        log_error(logger, "## Query %d: Error durante FLUSH previo al COMMIT",obtener_query_id());
        return st;
    }

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_COMMIT);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    
    protocolo_socket resp = recibir_paquete_ok(socket_storage);
    close(socket_storage);
    if (resp == OK) {
        memoria_agregar_commit(archivo, tag);
        log_info(logger, "## Query %d: COMMIT exitoso", obtener_query_id());
        return QI_OK;
    }

    log_error(logger, "## Query %d: Error en COMMIT", obtener_query_id());
    enviar_error_a_master(WORKER_FINALIZACION,"Error en COMMIT");
    return QI_ERR_PARSE;
}


qi_status_t ejecutar_FLUSH(char* archivo, char* tag) {
    log_info(logger, "## Query %d: Ejecutando FLUSH %s:%s",obtener_query_id(), archivo, tag);

    qi_status_t st = ejecutar_FLUSH_memoria(archivo, tag);

    if (st == QI_OK)
        log_info(logger, "## Query %d: FLUSH exitoso", obtener_query_id());
    else
        log_error(logger, "## Query %d: Error en FLUSH", obtener_query_id());

    return st;
}
qi_status_t ejecutar_DELETE(char* archivo, char* tag) {
    log_info(logger, "## Query %d: Ejecutando DELETE %s:%s", obtener_query_id(), archivo, tag);

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_DELETE);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);



    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    close(socket_storage);
    if (respuesta == OK) {
        log_info(logger, "## Query %d: DELETE exitoso", obtener_query_id());
        memoria_invalidar_file_tag_completo(archivo, tag);
        return QI_OK;
    } else {
        log_error(logger, "## Query %d: Error en DELETE", obtener_query_id());
        enviar_error_a_master(WORKER_FINALIZACION,"Error en DELETE");
        return QI_ERR_PARSE;
    }

}
int enviar_peticion_a_storage(t_paquete * paquete){
    int socket_storage;
    
    do
	{
		socket_storage = crear_conexion(ip_storage, puerto_storage);
		sleep(1);
        log_debug(logger,"Intentando conectar a STORAGE");        
	}while(socket_storage == -1);

    enviar_paquete(paquete, socket_storage);

    return socket_storage;
}

int enviar_error_a_master(protocolo_socket codigo,char* error){
    t_paquete* paquete = crear_paquete(codigo);
    agregar_a_paquete(paquete, error, strlen(error) + 1);
    enviar_paquete(paquete, socket_master);
    eliminar_paquete(paquete);
}