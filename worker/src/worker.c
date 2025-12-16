#include <worker.h>

int query_id;
extern char * query_path;
extern int program_counter;
sem_t * sem_hay_query;
sem_t * sem_desalojo_waiter;
sem_t * sem_flush_finalizado;
list_struct_t * lista_queries;
pthread_mutex_t * mutex_query_id;

int socket_storage;


void inicializar_memoria_interna(int tam_total, int tam_pagina);

int main(int argc, char* argv[]) {

    if (argc < 3){
        printf("Error en los argumentos de la archivo");
        return EXIT_FAILURE;
    }
    
    archivo_config = argv[1];
    id_worker = atoi(argv[2]);

    char * loggerFile = malloc (strlen(archivo_config) + strlen(".log")+1);

    sprintf(loggerFile, "%s.log", archivo_config);
    
    logger = log_create(loggerFile, "WORKER", 1, current_log_level);

    levantarStorage();
    levantarConfig(archivo_config);
    inicializarWorker();
    return 0;
}

void inicializarWorker(){

    

    pthread_t tid_conexion_master;
    pthread_t tid_conexion_storage;
    pthread_t tid_desalojo_master;

    sem_hay_query = inicializarSem(0);
    sem_desalojo_waiter = inicializarSem(0);
    sem_flush_finalizado = inicializarSem(1);

    mutex_query_id = inicializarMutex();

    lista_queries = inicializarLista();

    pthread_create(&tid_conexion_storage, NULL, conexion_cliente_storage, NULL);
    pthread_join(tid_conexion_storage, NULL);

    pthread_create(&tid_conexion_master, NULL, conexion_cliente_master, NULL);
    pthread_detach(tid_conexion_master);

    loop_principal();
    
}

void levantarConfig(char* archivo_config){
    char * path = malloc (strlen(archivo_config)+strlen(".config")+strlen("./")+1);
    
    strcpy(path, "./");
    strcat(path, archivo_config);
    strcat(path, ".config");

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
    char * path = malloc (strlen(archivo_config)+strlen(".config")+strlen("./")+1);
    
    strcpy(path, "./");
    strcat(path, archivo_config);
    strcat(path, ".config");
    config = config_create(path);
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    ip_storage = config_get_string_value(config, "IP_STORAGE");
    puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");

}

int obtener_query_id(){
    
    pthread_mutex_lock(mutex_query_id);
    int returnvalue = query_id;
    pthread_mutex_unlock(mutex_query_id);

    return returnvalue;
}

void setear_query_id(int value){
    
    pthread_mutex_lock(mutex_query_id);
    query_id = value;
    pthread_mutex_unlock(mutex_query_id);

}

void *conexion_cliente_master(void *args){
    
	do
	{
		socket_master = crear_conexion(ip_master, puerto_master);
		sleep(1);
        log_debug(logger,"Intentando conectar a MASTER");        
        
	}while(socket_master == -1);

    log_info(logger, "Conexión al Master exitosa. IP: <%s>, Puerto: <%s>",ip_master, puerto_master);
    log_info(logger, "Solicitud de ejecución de Worker ID:  <%d>", id_worker);

    t_paquete *paquete_send = crear_paquete(PARAMETROS_WORKER);
    agregar_a_paquete(paquete_send, &(id_worker), sizeof(int));
    enviar_paquete(paquete_send, socket_master);

    while(1){
        int cod_op = recibir_operacion(socket_master);

        t_list * paquete_recv;

        switch (cod_op)
        {
        case EXEC_QUERY:
            sem_wait(sem_flush_finalizado);
            paquete_recv = recibir_paquete(socket_master);
            //hay que crear una global o guardarlo en algun lado
            setear_query_id(*(int *) list_remove(paquete_recv, 0));
            query_path = list_remove(paquete_recv, 0);
            program_counter = *(int *) list_remove(paquete_recv, 0);

            list_destroy_and_destroy_elements(paquete_recv, free);

            //aca guardamos el query entero en una t_list

            pthread_mutex_lock(lista_queries->mutex);
            list_destroy_and_destroy_elements(lista_queries->lista, free);
            lista_queries->lista = list_create();

            char * temp_query_path = malloc(strlen(query_path)+strlen(Path_Queries)+2);
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

            t_list * paquete;
            paquete = recibir_paquete(socket_master);

            list_destroy_and_destroy_elements(paquete, free);

            break;

        default:
            log_error(logger, "Codigo de operacion inesperado en handler_worker %d", cod_op);

        }
    }
    
    return (void *)EXIT_SUCCESS;
}

void *conexion_cliente_storage (void *args){
	socket_storage;
    
    do
	{
		socket_storage = crear_conexion(ip_storage, puerto_storage);
		sleep(1);
        log_debug(logger,"Intentando conectar a STORAGE");        
        
	}while(socket_storage == -1);

    log_info(logger, "Conexión al Storage exitoso. IP: <%s>, Puerto: <%s>",ip_storage, puerto_storage);
    log_info(logger, "Solicitud de ejecución de Worker ID:  <%d>", id_worker);

    parametros_storage(socket_storage);
    
    return (void *)EXIT_SUCCESS; 
}


void parametros_storage(int socket_storage){
    t_paquete *paquete_send = crear_paquete(PARAMETROS_STORAGE);
    agregar_a_paquete(paquete_send, &id_worker, sizeof(int));
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
