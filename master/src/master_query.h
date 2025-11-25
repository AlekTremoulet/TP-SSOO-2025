#ifndef MASTER_QUERY_
#define MASTER_QUERY_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

typedef struct {
    int id_query;
    int prioridad;
    char *archivo;   // copia del path que manda el Query (uso strdup)
    int pc;
    int socket_qc;   // socket del Query para avisarle el final
} query_t;

extern char* puerto;
char * ArchivoQueryInst(char * path_query);

void query_enviar_lectura(query_t *q, void *buffer, int size);

void query_finalizar(query_t *q, char *motivo);

#endif