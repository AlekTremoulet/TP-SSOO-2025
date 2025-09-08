#include <master.h>



int main(int argc, char* argv[]) {
    pthread_t tid_server_mh_query;
    config = config_create("./master.config");
    logger = log_create("master.log", "master", 1, current_log_level);
    levantarConfig();
    pthread_create(&tid_server_mh_query, NULL, server_mh_query, NULL);
    log_destroy(logger);
    pthread_join(tid_server_mh_query, NULL);
    return 0;
}


void levantarConfig(){

    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto_query = config_get_string_value(config, "PUERTO_ESCUCHA");
}


void *server_mh_query(void *args){
    int server = iniciar_servidor(puerto_query);
    protocolo_socket cod_op;
    while(esperar_cliente(server)){
        cod_op = recibir_operacion(puerto_query);
    }

}