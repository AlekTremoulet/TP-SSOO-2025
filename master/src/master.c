#include <master.h>

t_log *logger = NULL;
t_config *config = NULL;
t_log_level current_log_level;
char *puerto = NULL;

char * algo_planificacion;

int id_query_actual;

// estructuras FIFO
typedef struct {
    int id_query;
    int prioridad;
    char *archivo;   // copia del path que manda el Query (uso strdup)
    int socket_qc;   // socket del Query para avisarle el final
} query_t;

typedef struct {
    int id;
    int socket_worker;   // socket del Worker para mandarle la tarea al Worker: path + id_query
} worker_t;

// colas thread-safe
list_struct_t *cola_ready_queries;   // elementos: query_t*
list_struct_t *workers_libres;       // elementos: worker_t*

pthread_mutex_t m_id;

int main(int argc, char* argv[]) {
    id_query_actual = 1;
    
    pthread_t tid_server_mh;
    pthread_t tid_planificador;

    
    levantarConfig(argv[argc-1]);

    // FIFO
    pthread_create(&tid_planificador, NULL, planificador, NULL);
    pthread_detach(tid_planificador); // pthread_detach =  el hilo se auto-limpia cuando termina
    // (como el planificador corre para siempre y nunca se lo va a esperar con join, se detacha y listo)

    pthread_create(&tid_server_mh, NULL, server_mh, NULL);
    pthread_join(tid_server_mh, NULL);

    log_destroy(logger);
    return 0;
}

void * planificador(void *_){
    if(!strcmp(algo_planificacion, "FIFO")){
        planificador_fifo();
    }else if (!strcmp(algo_planificacion, "PRIORIDADES")){
        planificador_prioridades();
    }
}

void levantarConfig(char *args){
    
    char * configpath = malloc(strlen(args)+strlen("./")+strlen(".config"));
    strcat(configpath, "./");
    strcat(configpath, args);
    strcat(configpath, ".config");
    config = config_create(configpath);
    char *value = config_get_string_value(config, "LOG_LEVEL");
    current_log_level = log_level_from_string(value);
    puerto= config_get_string_value(config, "PUERTO_ESCUCHA");
    logger = log_create("master.log", "MASTER", 1, current_log_level);
    log_debug(logger, "Config file: %s", configpath);

    algo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");

    // inicializo colas FIFO
    cola_ready_queries = inicializarLista();
    workers_libres     = inicializarLista();

    pthread_mutex_init(&m_id, NULL);

}

void *server_mh(void *args) { // Server Multi-hilo
    int server = iniciar_servidor(puerto);
    int socket_nuevo;

    // solo acepta y delega (un hilo por conexion)
    while ((socket_nuevo = esperar_cliente(server))) {
        int *arg = malloc(sizeof(int));
        *arg = socket_nuevo;

        pthread_t th;
        pthread_create(&th, NULL, handler_cliente, arg);
        pthread_detach(th);
    }
    return (void*)EXIT_SUCCESS;
}

// atiendo UNA conexion entrante (puede ser Query o Worker) y dejo el socket abierto
// en QUERY: encolo para FIFO - en WORKER: marco como libre
void *handler_cliente(void *arg) {
    int socket_nuevo = *(int*)arg;
    free(arg);

    protocolo_socket cod_op = recibir_operacion(socket_nuevo);
    if (cod_op <= 0) {
        log_error(logger, "Conexión entrante cerrada antes de operar");
        close(socket_nuevo);
        return NULL;
    }

    switch (cod_op) {
        case PARAMETROS_QUERY: {
            t_list *paquete_recv = recibir_paquete(socket_nuevo);

            // path (char*)
            char *archivo_recv = list_remove(paquete_recv, 0);

            // prioridad: viene como void* a un int -> casteo y desreferencio
            int *prioridad_ptr = list_remove(paquete_recv, 0);
            int prioridad = *prioridad_ptr;

            int id_asignado;    // valor inicial
            pthread_mutex_lock(&m_id);
            id_asignado = id_query_actual++;
            pthread_mutex_unlock(&m_id);

            log_info(logger,
                "Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprocesamiento <CANTIDAD>",
                archivo_recv, prioridad, id_asignado);

            enviar_paquete_ok(socket_nuevo);

            // FIFO - armo la query para la cola READY
            query_t *q = malloc(sizeof(query_t));
            q->archivo   = strdup(archivo_recv);   // me quedo una copia propia
            q->prioridad = prioridad;
            q->id_query  = id_asignado;
            q->socket_qc = socket_nuevo;           // guardo el socket del QC

            // encolo al final -> FIFO
            pthread_mutex_lock(cola_ready_queries->mutex);
            list_add(cola_ready_queries->lista, q);
            pthread_mutex_unlock(cola_ready_queries->mutex);
            sem_post(cola_ready_queries->sem);
            // FIFO

            // libero lo deserializado (archivo_recv y prioridad_ptr)
            list_destroy_and_destroy_elements(paquete_recv, free);
            break;
        }

        case PARAMETROS_WORKER: {
            t_list *paquete_recv = recibir_paquete(socket_nuevo);
            int *id_ptr = list_remove(paquete_recv, 0);
            int worker_id = *id_ptr;

            log_info(logger, "Se conecta un Worker con Id: <%d> ", worker_id);

            enviar_paquete_ok(socket_nuevo);

            // FIFO - registro al worker como libre
            worker_t *w = malloc(sizeof(worker_t));
            w->id            = worker_id;
            w->socket_worker = socket_nuevo;       // guardo la conexion al worker

            pthread_mutex_lock(workers_libres->mutex);
            list_add(workers_libres->lista, w);
            pthread_mutex_unlock(workers_libres->mutex);
            sem_post(workers_libres->sem);
            // FIFO

            list_destroy_and_destroy_elements(paquete_recv, free);
            break;
        }

        // resultado
        case DEVOLUCION_WORKER: {
            // TODO: leer (id_query, status) del paquete
            // TODO: re-encolar este worker en workers_libres
            // TODO: buscar socket_qc por id_query y enviar DEVOLUCION_QUERY
            break;
        }

        default: {
            // consumo payload si vino algo, para no dejar basura
            t_list *basura = recibir_paquete(socket_nuevo);
            if (basura) list_destroy_and_destroy_elements(basura, free);
            log_error(logger, "Se recibio un protocolo inesperado (%d)", cod_op);
            close(socket_nuevo);
            break;
        }
    }

    // el socket queda abierto: lo necesito para hablar con el Query/Worker.
    return NULL;
}


// ENUNCIADO FIFO:
// Las ejecuciones de las Queries se irán asignando a cada Worker por orden de llegada.
// Una vez que todos los Workers tengan una Query asignada se encolará el resto de las Queries en el estado READY.


// planificador FIFO: toma la primera query READY y el primer worker libre
// y le envia al worker (path, id_query)
void planificador_fifo() {
    while (1) {
        // espero hasta tener al menos 1 worker libre y 1 query en READY
        sem_wait(workers_libres->sem);
        sem_wait(cola_ready_queries->sem);

        // saco el primero de cada cola
        pthread_mutex_lock(workers_libres->mutex);
        worker_t *w = list_remove(workers_libres->lista, 0);
        pthread_mutex_unlock(workers_libres->mutex);

        pthread_mutex_lock(cola_ready_queries->mutex);
        query_t *q = list_remove(cola_ready_queries->lista, 0);
        pthread_mutex_unlock(cola_ready_queries->mutex);

        if (w == NULL || q == NULL) {
            // si no hay elementos, sigo
            continue;
        }

        // envio la asignacion al Worker: path + id_query
        t_paquete *p = crear_paquete(PARAMETROS_QUERY);
        agregar_a_paquete(p, q->archivo, strlen(q->archivo) + 1);
        agregar_a_paquete(p, &(q->id_query), sizeof(int));
        enviar_paquete(p, w->socket_worker);
        eliminar_paquete(p);

        log_info(logger, "FIFO: Asignada Query id <%d> al Worker id <%d>",
                 q->id_query, w->id);

        free(w);
        free(q->archivo);
        free(q);

        // TODO: el worker queda ocupado hasta implementar la devolucion
    }
    return;
}

void planificador_prioridades(){
    log_error(logger, "El planificador de prioridades no esta definido todavia");
    return;
}

// TODO: manejar DEVOLUCION_WORKER (id_query, status)
//   - re-encolar al worker en workers_libres
//   - avisarle al Query por socket_qc con DEVOLUCION_QUERY(status)
//   - IMPORTANTE: hay que guardar una tabla id_query -> socket_qc para saber xq socket responder a cada query cuando el Worker termine
