#ifndef WORKER_MAIN_
#define WORKER_MAIN_

#include <worker_query_interpreter.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
int id_worker;

char* archivo_config;
int prioridad;

char* ip_master;
char* ip_storage;
char* puerto_master;
char* puerto_storage;
char *puerto_master_desalojo;
int socket_master;
int socket_desalojo;

char * Path_Queries;

int Tam_memoria;

int tam_pagina = 1; //Del storage BLOCK_SIZE

int Retardo_reemplazo;
char * Algorit_Remplazo;

pthread_mutex_t * mutex_flag_desalojo;
int flag_desalojo;

pthread_mutex_t * mutex_flag_query_cancel;
int flag_query_cancel;

void inicializarWorker();
void levantarConfig(char* archivo_config);
void levantarStorage();

int obtener_query_id();

void setear_query_id(int value);

void *conexion_cliente_master(void *args);
void *conexion_cliente_storage(void *args);


void *desalojo_check(void *args);

void parametros_storage(int socket_storage);


#endif
