#ifndef WORKER_MAIN_
#define WORKER_MAIN_

#include <worker_query_interpreter.h>

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
char *puerto_master_desalojo;
int socket_master;
int socket_desalojo;
int socket_storage;

pthread_mutex_t * mutex_flag_desalojo;
int flag_desalojo;

void inicializarWorker();
void levantarConfig(char* archivo_config);
void levantarStorage();

void *conexion_cliente_master(void *args);
void *conexion_cliente_storage(void *args);


void *desalojo_check(void *args);

#endif
