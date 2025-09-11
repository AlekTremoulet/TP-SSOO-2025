#include <storage.h>

int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_worker;

    levantarConfig();
    pthread_create(&tid_server_mh_worker, NULL, server_mh_worker, NULL);
    pthread_join(tid_server_mh_worker, NULL);

    log_destroy(logger);
    return 0;
}

void levantarConfig(){

    config = config_create("./storage.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto= config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("storage.log", "STORAGE", 1, current_log_level);

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
