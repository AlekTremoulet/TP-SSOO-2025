#include <query_utilities.h>

extern t_log *logger;
extern t_config *config;
extern t_log_level current_log_level;

char * ip_master;
char * puerto_master;
int socket_master;
char *nombre_master;

void inicializarQuery(){

    config = config_create("./query.config");
    levantarConfig();

    logger = log_create("query.log", "QUERY", 1, current_log_level);

    pthread_t tid_conexion_master;

    pthread_create(&tid_conexion_master, NULL, conexion_cliente_master, NULL);
    pthread_join(tid_conexion_master, NULL);


}
void levantarConfig(){

    
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);

    ip_master = config_get_string_value(config, "IP_MASTER");
    puerto_master = config_get_string_value(config, "PUERTO_MASTER");

}

void *conexion_cliente_master(void *args){
    
	do
	{
		socket_master = crear_conexion(ip_master, puerto_master);
		sleep(1);
        

	}while(socket_master == -1);
    log_info(logger, "Se realizó la conexion con master");
    
    t_paquete *paquete_send = crear_paquete(PARAMETROS_QUERY);
    agregar_a_paquete(paquete_send, nombre_master, strlen(nombre_master) + 1);//ACA TENDRIAN QUE PASARLE LOS 3 ARGS
    enviar_paquete(paquete_send, socket_master);
    
    return (void *)EXIT_SUCCESS;
}
