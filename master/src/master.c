#include <master.h>

t_log *logger = NULL;
t_config *config = NULL;
t_log_level current_log_level;
char *puerto = NULL;

pthread_mutex_t * mutex_nivel_multiprocesamiento;
int nivel_multiprocesamiento;
pthread_mutex_t * mutex_cantidad_workers;
int cantidad_workers;

char * algo_planificacion;

int id_query_actual;

int tiempo_aging_ms = 0;


// colas thread-safe
list_struct_t *cola_ready_queries;  // elementos: query_t*
list_struct_t *cola_exec_queries;   // elementos: query_t*

list_struct_t *workers_libres;      // elementos: worker_t*
list_struct_t *workers_busy;        // elementos: worker_t*

pthread_mutex_t m_id;

static void posible_desalojo(query_t *q);


int main(int argc, char* argv[]) {
    id_query_actual = 1;
    
    pthread_t tid_server_mh;
    pthread_t tid_planificador;

    
    levantarConfig(argv[argc-1]);

    inicializarEstructurasMaster();

    // AGING
    if (!strcmp(algo_planificacion, "PRIORIDADES") && tiempo_aging_ms > 0) {
        pthread_t tid_aging;
        pthread_create(&tid_aging, NULL, hilo_aging, NULL);
        pthread_detach(tid_aging);
        log_info(logger, "## Se inicia hilo de aging cada <%d> ms", tiempo_aging_ms);
    }

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
    //cola_ready_queries = inicializarLista();
    //workers_libres     = inicializarLista();

    // aging !!!
    if (config_has_property(config, "TIEMPO_AGING")) {
        tiempo_aging_ms = config_get_int_value(config, "TIEMPO_AGING");
    } else {
        tiempo_aging_ms = 0;
    }

    pthread_mutex_init(&m_id, NULL);

}

void inicializarEstructurasMaster(){
    cola_ready_queries = inicializarLista();
    cola_exec_queries = inicializarLista();
    workers_busy = inicializarLista();
    workers_libres = inicializarLista();
    mutex_nivel_multiprocesamiento = inicializarMutex();
    mutex_cantidad_workers = inicializarMutex();
}

void aumentar_nivel_multiprocesamiento(){
    pthread_mutex_lock(mutex_nivel_multiprocesamiento);
    nivel_multiprocesamiento++;
    pthread_mutex_unlock(mutex_nivel_multiprocesamiento);
}

void disminuir_nivel_multiprocesamiento(){
    pthread_mutex_lock(mutex_nivel_multiprocesamiento);
    nivel_multiprocesamiento--;
    pthread_mutex_unlock(mutex_nivel_multiprocesamiento);
}
int obtener_nivel_multiprocesamiento(){
    pthread_mutex_lock(mutex_nivel_multiprocesamiento);
    int return_value = nivel_multiprocesamiento;
    pthread_mutex_unlock(mutex_nivel_multiprocesamiento);

    return return_value;
}
void aumentar_cantidad_workers(){
    pthread_mutex_lock(mutex_cantidad_workers);
    cantidad_workers++;
    pthread_mutex_unlock(mutex_cantidad_workers);
    aumentar_nivel_multiprocesamiento();

}

void disminuir_cantidad_workers(){
    pthread_mutex_lock(mutex_cantidad_workers);
    cantidad_workers--;
    pthread_mutex_unlock(mutex_cantidad_workers);
    disminuir_nivel_multiprocesamiento();
}
int obtener_cantidad_workers(){
    pthread_mutex_lock(mutex_cantidad_workers);
    int return_value = cantidad_workers;
    pthread_mutex_unlock(mutex_cantidad_workers);

    return return_value;
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

            // prioridad: viene como void* a un int - casteo y desreferencio
            int *prioridad_ptr = list_remove(paquete_recv, 0);
            int prioridad = *prioridad_ptr;

            int id_asignado;    // valor inicial
            pthread_mutex_lock(&m_id);
            id_asignado = id_query_actual++;
            pthread_mutex_unlock(&m_id);

            enviar_paquete_ok(socket_nuevo);

            // FIFO - armo la query para la cola READY
            query_t *q = malloc(sizeof(query_t));
            q->archivo   = strdup(archivo_recv);   // me quedo una copia propia
            q->prioridad = prioridad;
            q->id_query  = id_asignado;
            q->socket_qc = socket_nuevo;           // guardo el socket del QC
            q->pc = 0;

            log_info(logger,
                "## Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprocesamiento <%d>",
                archivo_recv, prioridad, id_asignado, obtener_nivel_multiprocesamiento());

            posible_desalojo(q);
 
            // encolo al final -> FIFO
            encolar_query(cola_ready_queries, q, -1);
            // FIFO

            // libero lo deserializado (archivo_recv y prioridad_ptr)
            list_destroy_and_destroy_elements(paquete_recv, free);
            break;
        }

        case PARAMETROS_WORKER: {
            t_list *paquete_recv = recibir_paquete(socket_nuevo);
            int worker_id = *(int *)list_remove(paquete_recv, 0);

            // FIFO - registro al worker como libre
            worker_t *w = malloc(sizeof(worker_t));
            w->id            = worker_id;
            w->socket_worker = socket_nuevo;       // guardo la conexion al worker

            w->query_id = -1; // inicializo el w

            encolar_worker(workers_libres, w, -1);
            // FIFO

            aumentar_cantidad_workers();

            log_info(logger, "##Se conecta un Worker con Id: <%d> - Cantidad total de Workers: <%d> ", worker_id, obtener_cantidad_workers());

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

worker_t * buscar_worker_por_id(int id, list_struct_t * cola){

    pthread_mutex_lock(cola->mutex);

    worker_t * aux;
    t_list_iterator * iterator = list_iterator_create(cola->lista);

    while (list_iterator_has_next(iterator)){
        aux = list_iterator_next(iterator);
        if (aux->id == id){
            break;
        }
    }

    pthread_mutex_unlock(cola->mutex);

    return aux;
}

void encolar_worker(list_struct_t *cola, worker_t *w, int index){
    
    pthread_mutex_lock(cola->mutex);
    //esto cambia index al real index del ultimo elemento, ya que list_add_in_index -1 no lo pone al final, vaya a saber quien porque. La documentacion de list.h no explica esto
    if (index == -1){
        if (list_is_empty(cola->lista)){
            index = 0;
        }else{
            worker_t * w_aux;
            t_list_iterator *iterator = list_iterator_create(cola->lista);
            while(list_iterator_has_next(iterator)){
                w_aux = list_iterator_next(iterator);
            }
            index = list_iterator_index(iterator)+1;
            list_iterator_destroy(iterator);
        }
    }
    
    
    list_add_in_index(cola->lista, index, w);
    pthread_mutex_unlock(cola->mutex);
    sem_post(cola->sem);
    return;
}
void encolar_query(list_struct_t *cola, query_t *q, int index){
    
    pthread_mutex_lock(cola->mutex);
    //esto cambia index al real index del ultimo elemento, ya que list_add_in_index -1 no lo pone al final, vaya a saber quien porque. La documentacion de list.h no explica esto
    if (index == -1){
        if (list_is_empty(cola->lista)){
            index = 0;
        }else{
            query_t * q_aux;
            t_list_iterator *iterator = list_iterator_create(cola->lista);
            while(list_iterator_has_next(iterator)){
                q_aux = list_iterator_next(iterator);
            }
            index = list_iterator_index(iterator)+1;
            list_iterator_destroy(iterator);
        }
    }
    
    
    list_add_in_index(cola->lista, index, q);
    pthread_mutex_unlock(cola->mutex);
    sem_post(cola->sem);
    return;
}
worker_t *desencolar_worker(list_struct_t *cola, int index){
    pthread_mutex_lock(cola->mutex);
    worker_t *w = list_remove(cola->lista, index);
    pthread_mutex_unlock(cola->mutex);
    return w;
}
query_t *desencolar_query(list_struct_t *cola, int index){
    pthread_mutex_lock(cola->mutex);
    query_t *q = list_remove(cola->lista, index);
    pthread_mutex_unlock(cola->mutex);
    return q;
}

static void encolar_query_en_exec(query_t *q) {
    encolar_query(cola_exec_queries, q, -1);
}

static void desencolar_query_de_exec(int id_query) {
    pthread_mutex_lock(cola_exec_queries->mutex);

    int size = list_size(cola_exec_queries->lista);
    for (int i = 0; i < size; i++) {
        query_t *q = list_get(cola_exec_queries->lista, i);
        if (q->id_query == id_query) {
            list_remove(cola_exec_queries->lista, i);
            break;
        }
    }

    pthread_mutex_unlock(cola_exec_queries->mutex);
}

// peor Q = numero mas grande
// no la saca de la cola, solo devuelve el puntero (o NULL si no hay nada)
static query_t *obtener_peor_query_exec(void) {
    pthread_mutex_lock(cola_exec_queries->mutex);

    int size = list_size(cola_exec_queries->lista);
    if (size == 0) {
        pthread_mutex_unlock(cola_exec_queries->mutex);
        return NULL;
    }

    int indice_peor = 0;
    query_t *peor_query = list_get(cola_exec_queries->lista, 0);

    for (int i = 1; i < size; i++) {
        query_t *q = list_get(cola_exec_queries->lista, i);

        // mayor numero = peor prioridad
        if (q->prioridad > peor_query->prioridad) {
            peor_query = q;
            indice_peor = i;
        }
    }

    log_debug(logger, "Peor Query en EXEC: id <%d> prioridad <%d> (indice %d)", peor_query->id_query, peor_query->prioridad, indice_peor);

    pthread_mutex_unlock(cola_exec_queries->mutex);
    return peor_query;
} // por ej. si llega una Q con prioridad mejor y no hay worker libre se desaloja la peor en EXEC

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
        worker_t *w = desencolar_worker(workers_libres,0);
        query_t *q = desencolar_query(cola_ready_queries,0);

        // marco al W como ocupado, cuando lo desencolo
        //encolar_worker(workers_busy, w, -1);

        if (w == NULL || q == NULL) {
            // si no hay elementos, sigo
            continue;
        }

        /*
        NOTA: creo que lo mejor es levantar un thread por cada par de worker query que se ejecute, osea que el codigo de abajo correria dentro de esa funcion de thread.
        
        -Thread principal de planificador       (asigna workers cuando llegan queries)
        \-thread de worker                      (con su query asignado)
        \-otro thread de worker                 (con su query asignado)

        El thread del worker queda esperando una respuesta del worker. En caso de que haya desalojo se le envia un paquete al worker que atiende asincronicamente,
        luego en el thread de worker atajamos el motivo de devolucion. Liberamos el worker y ponemos el query en la lista de ready o lo finalizamos
        */

         // estructura que voy a pasarle al thread
        datos_worker_query_t *dwq = malloc(sizeof(datos_worker_query_t));
        dwq->worker = w;
        dwq->query  = q;

        encolar_query_en_exec(q); // Q en EXEC

        encolar_worker(workers_busy, w, -1);
        log_debug(logger, "Worker %d se encola en lista busy", w->id);

        // creo el thread para worker+query
        pthread_t th_wq;
        pthread_create(&th_wq, NULL, hilo_worker_query, dwq);
        pthread_detach(th_wq);

        // el planificador no se queda esperando
    }
    return;
}

void *hilo_worker_query(void *arg) {

    // recibo el paquete de datos
    datos_worker_query_t *dwq = (datos_worker_query_t *)arg;
    worker_t *w = dwq->worker;
    query_t  *q = dwq->query;

    w->query_id = q->id_query; // este w ejecuta esta q

    log_info(logger, "## Se envía la Query <%d> (PRIORIDAD <%d>) al Worker <%d>", q->id_query, q->prioridad, w->id);

    // le mando al W el path + id_query
    t_paquete *p = crear_paquete(EXEC_QUERY);
    agregar_a_paquete(p, &(q->id_query), sizeof(int));        // envio id
    agregar_a_paquete(p, q->archivo, strlen(q->archivo) + 1); // envio path
    agregar_a_paquete(p, &(q->pc), sizeof(int)); //envio pc
    enviar_paquete(p, w->socket_worker);
    eliminar_paquete(p);

    // respuesta del W
    while(1){
        protocolo_socket cod = recibir_operacion(w->socket_worker);

        if (cod == -1){
            desencolar_query_de_exec(q->id_query); // si el W se desconecto mientras ejecutaba la Q

            disminuir_cantidad_workers();
            free(w);
            query_finalizar(q, "desconexion de worker");
            log_info(logger, "## Se desconecta el Worker %d - Se finaliza la Query %d - Cantidad total de Workers: %d", w->id, q->id_query, obtener_cantidad_workers());
        
            return NULL; // se corta el hilo
        }

        switch (cod) {

            case OP_READ: {

                t_list* paquete = recibir_paquete(w->socket_worker);

                char * archivo = list_remove(paquete, 0);
                char * tag = list_remove(paquete, 0);
                char *buffer = list_remove(paquete, 0);

                log_info(logger, "## Se envía un mensaje de lectura de la Query <%d> en el Worker <%d> al Query Control", q->id_query, w->id);

                query_enviar_lectura(q, archivo, tag, buffer);

                list_destroy_and_destroy_elements(paquete, free);

                break;
            }

            case WORKER_FINALIZACION: {

                t_list *paquete = recibir_paquete(w->socket_worker);
                char * motivo = list_remove(paquete, 0);
            
                log_info(logger, "## Se terminó la Query <%d> en el Worker <%d>", q->id_query, w->id);

                // termino una query

                desencolar_query_de_exec(q->id_query);

                // el worker ya no ejecuta ninguna query
                w->query_id = -1;

                // reencolar worker como libre
                encolar_worker(workers_libres, w, -1);
                log_debug(logger, "Worker %d se encola en lista ready", w->id);

                query_finalizar(q, motivo);

                free(q->archivo);
                free(q);
                free(dwq);

                //en caso de no-lectura se corta el while
                return NULL;
            }

            case DESALOJO: {
                
                t_list *paquete = recibir_paquete(w->socket_worker);

                //guardo el PC
                q->pc = *(int *)list_remove(paquete, 0);

                if (paquete != NULL) {
                    list_destroy_and_destroy_elements(paquete, free);
                }

                log_info(logger, "## Se desaloja la Query <%d> (<%d>) del Worker <%d> - Motivo: <PRIORIDAD>", q->id_query, q->prioridad, w->id);

                desencolar_query_de_exec(q->id_query);

                // reencolo la query en READY para que vuelva a planificarse
                encolar_query(cola_ready_queries, q, -1);

                // el worker deja de tener query asignada
                w->query_id = -1;

                // reencolo el worker como libre
                encolar_worker(workers_libres, w, -1);

                free(dwq);

                //en caso de no-lectura se corta el while
                return NULL;
            }   

            default:
                log_error(logger, "Worker <%d> devolvió un código inesperado <%d>", w->id, cod);
                
                free(q->archivo);
                free(q);
                free(dwq);

                w->query_id = -1; 
                encolar_worker(workers_libres, w, -1);

                break;
        }
    }
    

    // el worker no se libera, solo se reencola
    return NULL;
}

// hilo de aging: cada TIEMPO_AGING baja en 1 la prioridad de las Q en READY (si > 0)
void *hilo_aging(void *arg) {
    while (1) {
        if (tiempo_aging_ms <= 0) {
            // no hay tiempo configurado
            // usleep(1000 * 100); ¿??
            continue;
        }

        usleep(tiempo_aging_ms * 1000); // espero TIEMPO_AGING ms

        pthread_mutex_lock(cola_ready_queries->mutex);

        t_list_iterator *iterador_ready = list_iterator_create(cola_ready_queries->lista);

        while (list_iterator_has_next(iterador_ready)) {
            query_t *q = list_iterator_next(iterador_ready);

            if (q->prioridad > 0) {
                int prioridad_anterior = q->prioridad;
                q->prioridad--;

                log_info(logger, "##%d Cambio de prioridad: %d - %d", q->id_query, prioridad_anterior, q->prioridad);
            }
        }

        list_iterator_destroy(iterador_ready);
        pthread_mutex_unlock(cola_ready_queries->mutex);
    }

    return NULL;
}

// saca de cola_ready_queries la query con mejor prioridad (osea el numero mas chico)
static query_t *sacar_mejor_query_ready(void) {
    pthread_mutex_lock(cola_ready_queries->mutex);

    int size = list_size(cola_ready_queries->lista);
    if (size == 0) {
        pthread_mutex_unlock(cola_ready_queries->mutex);
        return NULL;
    }

    int indice_prioridad_minima = 0; 
    query_t *query_con_mejor_prioridad = list_get(cola_ready_queries->lista, 0); // asumo que es la de la primera posicion

    for (int i = 1; i < size; i++) {
        query_t *q = list_get(cola_ready_queries->lista, i);
        // menor numero = mejor prioridad
        if (q->prioridad < query_con_mejor_prioridad->prioridad) {
            query_con_mejor_prioridad = q;
            indice_prioridad_minima = i;
        }
    }

    query_t *resultado = list_remove(cola_ready_queries->lista, indice_prioridad_minima);

    pthread_mutex_unlock(cola_ready_queries->mutex);
    return resultado;
}

/* void planificador_prioridades(){
    log_error(logger, "El planificador de prioridades no esta definido todavia");
    return;
} */

void planificador_prioridades() {
    while (1) {
        // espero hasta tener un W libre y una Q en READY
        sem_wait(workers_libres->sem);
        sem_wait(cola_ready_queries->sem);

        query_t *q = sacar_mejor_query_ready(); // numero mas chico=mejor prioridad
        worker_t *w = desencolar_worker(workers_libres, 0);

        if (w == NULL || q == NULL) {
            if (w != NULL) {
                encolar_worker(workers_libres, w, -1);
                sem_post(workers_libres->sem);
            }
            // no hay Q
            continue;
        }

        // hilo worker+query
        datos_worker_query_t *dwq = malloc(sizeof(datos_worker_query_t));
        dwq->worker = w;
        dwq->query  = q;

        encolar_query_en_exec(q); // Q en EXEC

        // marco al W como ocupado
        encolar_worker(workers_busy, w, -1);
        log_debug(logger, "Worker %d se encola en lista busy", w->id);


        log_info(logger, "PRIORIDADES: Se envía la Query <%d> (PRIORIDAD <%d>) al Worker <%d>", q->id_query, q->prioridad, w->id);

        // creo hilo worker+query
        pthread_t th_wq;
        pthread_create(&th_wq, NULL, hilo_worker_query, dwq);
        pthread_detach(th_wq);
    }
}

static void posible_desalojo(query_t *q_nueva) {
    if (strcmp(algo_planificacion, "PRIORIDADES") != 0) {
        return;
    }

    // si hay un w libre no desalojo
    if (hay_workers_libres()) {
        log_info(logger, "Q <%d> (prio %d). Hay workers libres", q_nueva->id_query, q_nueva->prioridad);
        return;
    }

    query_t *peor = obtener_peor_query_exec();

    if (peor == NULL) {
        log_info(logger, "Q <%d> (prio %d). EXEC vacio, no hay desalojo.",  q_nueva->id_query, q_nueva->prioridad);
        return;
    }

    log_info(logger, "Q <%d> (prio %d). Peor en EXEC: <%d> (prio %d).", q_nueva->id_query, q_nueva->prioridad, peor->id_query, peor->prioridad);

    // si la nueva no es mejor que la peor en exec (menor numero = mejor prioridad)
    if (q_nueva->prioridad >= peor->prioridad) {
        log_info(logger, "La Query <%d> no mejora la prioridad de la peor en EXEC. No se desaloja.", q_nueva->id_query);
        return;
    }

    // la nueva es mejor que la peor en exec y no hay workers libres
    worker_t *w_a_desalojar = buscar_worker_por_query_id(peor->id_query);
    if (w_a_desalojar == NULL) {
        log_error(logger, "No hay Worker que ejecute la Query <%d> para desalojar.", peor->id_query);
        return;
    }

    log_info(logger, "Se desaloja la Query <%d> del Worker <%d> para ejecutar la nueva Query <%d> (prio mejor).", peor->id_query, w_a_desalojar->id, q_nueva->id_query);

    t_paquete *paquete_desalojo = crear_paquete(DESALOJO);
    enviar_paquete(paquete_desalojo, w_a_desalojar->socket_worker);
    eliminar_paquete(paquete_desalojo);
}

static bool hay_workers_libres(void) {
    bool hay_w_libres = false;

    pthread_mutex_lock(workers_libres->mutex);
    hay_w_libres = !list_is_empty(workers_libres->lista);
    pthread_mutex_unlock(workers_libres->mutex);

    return hay_w_libres;
}

static worker_t *buscar_worker_por_query_id(int id_query) {
    worker_t *w_encontrado = NULL;

    pthread_mutex_lock(workers_busy->mutex);

    int size = list_size(workers_busy->lista);
    for (int i = 0; i < size; i++) {
        worker_t *w = list_get(workers_busy->lista, i);
        if (w->query_id == id_query) {
            w_encontrado = w;
            break;
        }
    }

    pthread_mutex_unlock(workers_busy->mutex);

    return w_encontrado;
}
