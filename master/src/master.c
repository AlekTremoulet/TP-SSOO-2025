#include <master.h>



int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_query;
    pthread_t tid_server_mh_worker;

    levantarConfig();
    pthread_create(&tid_server_mh_query, NULL, server_mh_query, NULL);
    pthread_create(&tid_server_mh_worker, NULL, server_mh_worker, NULL);
    pthread_join(tid_server_mh_query, NULL);
    pthread_join(tid_server_mh_worker, NULL);
    log_destroy(logger);
    return 0;
}


void levantarConfig(){

    config = config_create("./master.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto= config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("master.log", "MASTER", 1, current_log_level);

}


void *server_mh_query(void *args){ // Server Multi-hilo 
    int server = iniciar_servidor(puerto);
    protocolo_socket cod_op;

    t_list *paquete_recv;
    int socket_nuevo;
    // char * archivo_actual;
    // int prioridad_query_actual;
    parametros_query parametros_recibidos;

    while((socket_nuevo = esperar_cliente(server))){
        
        cod_op = recibir_operacion(socket_nuevo);
        
        if(cod_op != PARAMETROS_QUERY){
            log_error(logger, "Se recibio un protocolo inesperado de la QUERY");
            return (void*)EXIT_FAILURE;
        }
        paquete_recv = recibir_paquete(socket_nuevo);

        parametros_recibidos.archivo = list_remove(paquete_recv, 0);
        int* prioridad_ptr = list_remove(paquete_recv, 0);
        parametros_recibidos.prioridad = *prioridad_ptr;
        parametros_recibidos.id_query = id_query_actual;
        id_query_actual ++;
        parametros_recibidos.estado = READY_Q;

        log_info(logger, "Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprocesamiento <CANTIDAD>",parametros_recibidos.archivo,parametros_recibidos.prioridad,parametros_recibidos.id_query);
        enviar_paquete_ok(socket_nuevo);

    }
    return (void *)EXIT_SUCCESS;

}

void server_mh_worker() {
    int server = iniciar_servidor(puerto);
    protocolo_socket cod_op;

    t_list *paquete_recv;
    int socket_nuevo;
    parametros_worker parametros_recibidos;

    while((socket_nuevo = esperar_cliente(server))){
        
        cod_op = recibir_operacion(socket_nuevo);
        
        if(cod_op != PARAMETROS_WORKER){
            log_error(logger, "Se recibio un protocolo inesperado del WORKER");
            return (void*)EXIT_FAILURE;
        }
        paquete_recv = recibir_paquete(socket_nuevo);

        parametros_recibidos.id  = list_remove(paquete_recv, 0);
        parametros_recibidos.pc  = list_remove(paquete_recv, 0);

        log_info(logger, "Se conecta un Worker con Id: <%d> con el PC: <%d>",parametros_recibidos.id, parametros_recibidos.pc);
        enviar_paquete_ok(socket_nuevo);

    }
    return (void *)EXIT_SUCCESS;

}