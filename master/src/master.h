#ifndef MASTER_MAIN_
#define MASTER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

extern t_log *logger;
extern t_config *config;
extern t_log_level current_log_level;
extern char * puerto;
extern int id_query_actual;

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
void planificador_fifo();
void planificador_prioridades();
void *planificador(void * args);

#endif