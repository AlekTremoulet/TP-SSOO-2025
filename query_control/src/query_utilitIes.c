#include <query_utilities.h>

extern t_log *logger;
extern t_config *config;
extern t_log_level current_log_level;

char * ip_master;
char * puerto_master;
int socket_master;
char * nombre_master = "TEST";
extern parametros_query parametros_a_enviar;

void inicializarQuery(){
    
    logger = log_create("query.log", "QUERY", 1, current_log_level);

    pthread_t tid_conexion_master;

    pthread_create(&tid_conexion_master, NULL, conexion_cliente_master, NULL);
    pthread_join(tid_conexion_master, NULL);


}
void levantarConfig(char* archivo_config){
    char path[64];
    snprintf(path, sizeof(path), "./%s", archivo_config);
    config = config_create(path);
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

    log_info(logger, "Conexión al Master exitosa. IP: <%s>, Puerto: <%s>",ip_master, puerto_master);
    log_info(logger, "## Solicitud de ejecución de Query: <%s>, prioridad: <%d>",parametros_a_enviar.archivo, parametros_a_enviar.prioridad);

    t_paquete *paquete_send = crear_paquete(PARAMETROS_QUERY);
    agregar_a_paquete(paquete_send, parametros_a_enviar.archivo, strlen(parametros_a_enviar.archivo) + 1);
    agregar_a_paquete(paquete_send, &(parametros_a_enviar.prioridad), sizeof(int));
    enviar_paquete(paquete_send, socket_master);

    recibir_paquete_ok(socket_master);
    
    while(1){
        
        protocolo_socket cod_op = recibir_operacion(socket_master);
        t_list * paquete_recv;

        switch(cod_op){
            case QUERY_FINALIZACION:
                paquete_recv = recibir_paquete(socket_master);
                char * motivo = list_remove(paquete_recv, 0);
                log_info(logger, "## Query Finalizada - <%s> <%s>", motivo,parametros_a_enviar.archivo);
                return NULL;

            case QUERY_LECTURA:
                paquete_recv = recibir_paquete(socket_master);
                char * file = list_remove(paquete_recv, 0);
                char * tag = list_remove(paquete_recv, 0);
                char * contenido = list_remove(paquete_recv, 0);
                log_info(logger, "## Lectura realizada: File <%s:%s>, contenido: <%s>", file, tag, contenido);
                break;

            default:
                log_error(logger, "parametro desconocido recibido desde Master: %d", cod_op);
                return NULL;

        }

        
    }    

    return (void *)EXIT_SUCCESS;
}

