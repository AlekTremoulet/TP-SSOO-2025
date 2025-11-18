#ifndef WORKER_MAIN_
#define WORKER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
parametros_worker parametros_a_enviar;

char* archivo_config;
int prioridad;

char* ip_master;
char* ip_storage;
char* puerto_master;
char* puerto_storage;
int socket_master;
int socket_storage;

void inicializarWorker();
void levantarConfig(char* archivo_config);
void levantarStorage();
void *conexion_cliente_master(void *args);
void *conexion_cliente_storage(void *args);

#endif
