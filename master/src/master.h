#ifndef MASTER_MAIN_
#define MASTER_MAIN_

#include <master_query.h>

extern t_log *logger;
extern t_config *config;
extern t_log_level current_log_level;
extern char * puerto;
extern int id_query_actual;


// estructuras FIFO


typedef struct {
    int id;
    int socket_worker;   // socket del Worker para mandarle la tarea al Worker: path + id_query
} worker_t;

// los datos que necesito pasarle al thread
typedef struct {
    worker_t *worker;
    query_t  *query;
} datos_worker_query_t;

int main(int argc, char* argv[]);
void levantarConfig(char *args);
void aumentar_nivel_multiprocesamiento();
void disminuir_nivel_multiprocesamiento();
int obtener_nivel_multiprocesamiento();
void aumentar_cantidad_workers();
void inicializarEstructurasMaster();
void disminuir_cantidad_workers();
int obtener_cantidad_workers();
void *server_mh(void *args);
void *handler_cliente(void *arg);
void encolar_worker(list_struct_t *cola, worker_t *w, int index);
void encolar_query(list_struct_t *cola, query_t *q, int index);
worker_t *desencolar_worker(list_struct_t *cola, int index);
query_t *desencolar_query(list_struct_t *cola, int index);
static void desencolar_query_de_exec(int id_query);
static query_t *obtener_peor_query_exec(void);
void planificador_fifo();
static query_t *sacar_mejor_query_ready(void);
void planificador_prioridades();
void *hilo_aging(void *arg);
void *planificador(void * args);
void *hilo_worker_query(void *arg);

#endif