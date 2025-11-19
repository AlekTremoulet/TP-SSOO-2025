#include <worker.h>

int main(int argc, char* argv[]) {

    if (argc < 3){
        printf("Error en los argumentos de la archivo");
        return EXIT_FAILURE;
    }
    
    archivo_config = argv[1];
    parametros_a_enviar.id = atoi(argv[2]);

    levantarStorage();
    levantarConfig(archivo_config);
    inicializarWorker();
    return 0;
}

void inicializarWorker(){
    
    logger = log_create("worker.log", "WORKER", 1, current_log_level);

    pthread_t tid_conexion_master;
    pthread_t tid_conexion_storage;
    pthread_t tid_desalojo_master;

    pthread_create(&tid_conexion_storage, NULL, conexion_cliente_storage, NULL);
    pthread_join(tid_conexion_storage, NULL);

    pthread_create(&tid_conexion_master, NULL, conexion_cliente_master, NULL);
    pthread_join(tid_conexion_master, NULL);

    pthread_create(&tid_desalojo_master, NULL, desalojo_check, NULL);
    pthread_detach(tid_desalojo_master);

    loop_principal();
    
}

void levantarConfig(char* archivo_config){
    char path[64];
    snprintf(path, sizeof(path), "./%s", archivo_config);
    config = config_create(path);
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);

    ip_master = config_get_string_value(config, "IP_MASTER");
    puerto_master = config_get_string_value(config, "PUERTO_MASTER");
    puerto_master_desalojo = config_get_string_value(config, "PUERTO_MASTER_DESALOJO");
}
void levantarStorage(){
    config = config_create("./worker.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    ip_storage = config_get_string_value(config, "IP_STORAGE");
    puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");
}


void *desalojo_check(void * args){

    while(1){
        protocolo_socket cod_op = recibir_operacion(socket_desalojo);

        switch (cod_op)
        {
        case DESALOJO:
            setear_desalojo_flag(true);
            break;
        
        default:
            log_error(logger, "operacion desconocida en desalojo_check");
            break;
        }
    }
    
    return (void *)EXIT_SUCCESS;

}

void *conexion_cliente_master(void *args){
    
	do
	{
		socket_master = crear_conexion(ip_master, puerto_master);
		sleep(1);
        log_debug(logger,"Intentando conectar a MASTER");        
        
	}while(socket_master == -1);

    log_info(logger, "Conexión al Master exitosa. IP: <%s>, Puerto: <%s>",ip_master, puerto_master);
    log_info(logger, "Solicitud de ejecución de Worker ID:  <%d>", parametros_a_enviar.id);

    do
	{
		socket_desalojo = crear_conexion(ip_master, puerto_master_desalojo);
		sleep(1);
        
	}while(socket_master == -1);

    log_info(logger, "Conexión al Master-desalojo exitosa. IP: <%s>, Puerto: <%s>",ip_master, puerto_master_desalojo);

    t_paquete *paquete_send = crear_paquete(PARAMETROS_WORKER);
    agregar_a_paquete(paquete_send, &(parametros_a_enviar.id), sizeof(int));
    enviar_paquete(paquete_send, socket_master);
    
    return (void *)EXIT_SUCCESS;
}

void *conexion_cliente_storage (void *args){
	do
	{
		socket_storage = crear_conexion(ip_storage, puerto_storage);
		sleep(1);
        log_debug(logger,"Intentando conectar a STORAGE");        
        
	}while(socket_storage == -1);

    log_info(logger, "Conexión al Storage exitoso. IP: <%s>, Puerto: <%s>",ip_storage, puerto_storage);
    log_info(logger, "Solicitud de ejecución de Worker ID:  <%d>", parametros_a_enviar.id);

    
    return (void *)EXIT_SUCCESS; 
}