#include <master.h>

int id_query_actual;

// estructuras FIFO
typedef struct {
    int id_query;
    int prioridad;
    char *archivo;   // copia del path que manda el Query
    int socket_qc;   // socket del Query para avisarle el final
} query_t;

typedef struct {
    int id;
    int socket_worker;   // socket del Worker para mandarle la asignación
} worker_t;

// colas thread-safe
list_struct_t *cola_ready_queries;   // elementos: query_t*
list_struct_t *workers_libres;       // elementos: worker_t*

int main(int argc, char* argv[]) {
    id_query_actual = 1;
    
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

    // inicializo colas FIFO (listas con mutex + semaforo)
    cola_ready_queries = inicializarLista();
    workers_libres     = inicializarLista();

}

// atiendo UNA conexion entrante (puede ser Query o Worker) y dejo el socket abierto
// en QUERY: encolo para FIFO - en WORKER: marco como libre
static void *handler_cliente(void *arg) {
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

            int id_asignado = id_query_actual++;
            log_info(logger,
                "Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprocesamiento <CANTIDAD>",
                archivo_recv, prioridad, id_asignado);

            enviar_paquete_ok(socket_nuevo);

            // FIFO
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

            // FIFO
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