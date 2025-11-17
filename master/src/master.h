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
void inicializarEstructurasMaster();
void *server_mh(void *args);
void *handler_cliente(void *arg);
void planificador_fifo();
void planificador_prioridades();
void *planificador(void * args);

#endif