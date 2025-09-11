#include <master.h>



int main(int argc, char* argv[]) {
    pthread_t tid_server_mh;

    levantarConfig();
    pthread_create(&tid_server_mh, NULL, server_mh, NULL);
    pthread_join(tid_server_mh, NULL);

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


void *server_mh(void *args){ // Server Multi-hilo 
    int server = iniciar_servidor(puerto);
    protocolo_socket cod_op;

    t_list *paquete_recv;
    int socket_nuevo;
    // char * archivo_actual;
    // int prioridad_query_actual;
    parametros_query parametros_recibidos_query;
    parametros_worker parametros_recibidos_worker;

    while((socket_nuevo = esperar_cliente(server))){
        
        cod_op = recibir_operacion(socket_nuevo);
        switch (cod_op)
        {
        case PARAMETROS_QUERY:
            paquete_recv = recibir_paquete(socket_nuevo);
            parametros_recibidos_query.archivo = list_remove(paquete_recv, 0);
            int* prioridad_ptr = list_remove(paquete_recv, 0);
            parametros_recibidos_query.prioridad = *prioridad_ptr;
            parametros_recibidos_query.id_query = id_query_actual;
            id_query_actual ++;
            parametros_recibidos_query.estado = READY_Q;

            log_info(logger, "Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprocesamiento <CANTIDAD>",parametros_recibidos_query.archivo,parametros_recibidos_query.prioridad,parametros_recibidos_query.id_query);
            enviar_paquete_ok(socket_nuevo);
            break;
        case PARAMETROS_WORKER:
            paquete_recv = recibir_paquete(socket_nuevo);
            int* id_ptr = list_remove(paquete_recv, 0);
            parametros_recibidos_worker.id = *id_ptr;

            log_info(logger, "Se conecta un Worker con Id: <%d> ",parametros_recibidos_worker.id);
            enviar_paquete_ok(socket_nuevo);
            break;
        default:
                log_error(logger, "Se recibio un protocolo inesperado");
            break;
        }
        return (void *)EXIT_SUCCESS;
    }

}