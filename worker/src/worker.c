#include <worker.h>

extern int query_id;
extern char * query_path;
extern int program_counter;
sem_t * sem_hay_query;
sem_t * sem_desalojo_waiter;
list_struct_t * lista_queries;


void inicializar_memoria_interna(int tam_total, int tam_pagina);

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

    sem_hay_query = inicializarSem(0);
    sem_desalojo_waiter = inicializarSem(0);

    lista_queries = inicializarLista();

    pthread_create(&tid_conexion_storage, NULL, conexion_cliente_storage, NULL);
    pthread_join(tid_conexion_storage, NULL);

    pthread_create(&tid_conexion_master, NULL, conexion_cliente_master, NULL);
    pthread_detach(tid_conexion_master);

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

    Path_Queries = config_get_string_value(config, "PATH_SCRIPTS");
    Tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
    Retardo_reemplazo = config_get_int_value(config, "RETARDO_MEMORIA");
    Algorit_Remplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");

    

}
void levantarStorage(){
    config = config_create("./worker.config");
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    ip_storage = config_get_string_value(config, "IP_STORAGE");
    puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");

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

    t_paquete *paquete_send = crear_paquete(PARAMETROS_WORKER);
    agregar_a_paquete(paquete_send, &(parametros_a_enviar.id), sizeof(int));
    enviar_paquete(paquete_send, socket_master);

    while(1){
        int cod_op = recibir_operacion(socket_master);

        t_list * paquete_recv;

        switch (cod_op)
        {
        case EXEC_QUERY:
            paquete_recv = recibir_paquete(socket_master);
            //hay que crear una global o guardarlo en algun lado
            query_id = *(int *) list_remove(paquete_recv, 0);
            query_path = list_remove(paquete_recv, 0);
            program_counter = *(int *) list_remove(paquete_recv, 0);

            //aca guardamos el query entero en una t_list

            pthread_mutex_lock(lista_queries->mutex);
            list_destroy_and_destroy_elements(lista_queries->lista, free);
            lista_queries->lista = list_create();

            char * temp_query_path = malloc(strlen(query_path)+strlen(Path_Queries)+1);
            strcpy(temp_query_path, Path_Queries);
            strcat(temp_query_path, "/");
            strcat(temp_query_path, query_path);

            FILE *fp = fopen(temp_query_path, "r");
            if (!fp)
            {
                perror("Error opening file");
                return NULL;
            }
            char buffer[1024];

            while (fgets(buffer, sizeof(buffer), fp))
            {
                // duplicate the line so it persists
                char *line = strdup(buffer);
                if (!line)
                    break;

                list_add(lista_queries->lista, line);       // your library handles insertion
            }
            pthread_mutex_unlock(lista_queries->mutex);

            fclose(fp);

            log_info(logger, "## Query %d: Se recibe la Query. El path de operaciones es: %s", query_id, query_path);
            sem_post(sem_hay_query);

            break;
        
        case QUERY_FINALIZACION:

            pthread_mutex_lock(lista_queries->mutex);
            list_destroy_and_destroy_elements(lista_queries->lista, free);
            pthread_mutex_unlock(lista_queries->mutex);

            enviar_paquete_ok(socket_master);

            break;

        case DESALOJO:

            // en master: cuando hay un desalojo, se manda el paquete con cod_op DESALOJO, master espera un PC, luego envia un nuevo paquete EXEC_QUERY

            setear_desalojo_flag(true);

            pthread_mutex_lock(lista_queries->mutex);
            list_destroy_and_destroy_elements(lista_queries->lista, free);
            pthread_mutex_unlock(lista_queries->mutex);

            t_paquete * paquete_send = crear_paquete(DESALOJO);
            agregar_a_paquete(paquete_send, &program_counter, sizeof(int));

            enviar_paquete(paquete_send, socket_master);

            setear_desalojo_flag(false);
            sem_post(sem_desalojo_waiter);
            break;

        default:
            log_error(logger, "Codigo de operacion inesperado en handler_worker");

        }
    }
    
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

    parametros_storage(socket_storage);
    
    return (void *)EXIT_SUCCESS; 
}


void parametros_storage(int socket_storage){
    t_paquete *paquete_send = crear_paquete(PARAMETROS_STORAGE);
    enviar_paquete(paquete_send, socket_storage);
    eliminar_paquete(paquete_send);

    protocolo_socket cod_op = recibir_operacion(socket_storage);
    if (cod_op == PARAMETROS_STORAGE){
        t_list *paquete_recv = recibir_paquete(socket_storage);
        tam_pagina =  *(int*) list_remove(paquete_recv, 0); 
        log_debug(logger, "Parametros Storage:  <%d>",tam_pagina);

        inicializar_memoria_interna(Tam_memoria,tam_pagina);
    }
};
