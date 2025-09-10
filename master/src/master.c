#include <master.h>



int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_query;
    levantarConfig();
    pthread_create(&tid_server_mh_query, NULL, server_mh_query, NULL);
    pthread_join(tid_server_mh_query, NULL);
    log_destroy(logger);
    return 0;
}


void levantarConfig(){

    config = config_create("./master.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto_query = config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("master.log", "MASTER", 1, current_log_level);

}


void *server_mh_query(void *args){ // Server Multi-hilo 
    int server = iniciar_servidor(puerto_query);
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
        enviar_paquete_ok(socket_nuevo); // Hay que hacerlo con el worker

    }
    return (void *)EXIT_SUCCESS;

}