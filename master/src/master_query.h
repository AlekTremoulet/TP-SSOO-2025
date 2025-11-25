#ifndef MASTER_QUERY_
#define MASTER_QUERY_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

extern char* puerto;
char * ArchivoQueryInst(char * path_query);

query_enviar_lectura(query_t *q, void *buffer, int size);

query_finalizar(query_t *q, char *motivo);

#endif