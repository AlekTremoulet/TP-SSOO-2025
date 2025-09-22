#ifndef WORKER_QUERY_INTERPRETER_
#define WORKER_QUERY_INTERPRETER_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

typedef enum {
    QI_OK = 0, 
    QI_END, 
    QI_ERR_IO, 
    QI_ERR_PARSE, 
    QI_ERR_FUERA_LIMITE,
    QI_ERR_OP_NO_PERMITIDA, 
    QI_ERR_NO_ENCONTRADA, 
    QI_ERR_NO_HAY_ESPACIO
} qi_status_t;

void intepretar_Instruccion(t_instruccion instruccion);
static qi_status_t exec_CREATE(int qid, char* cadena);
static bool separar_nombre_y_tag(const char* cadena, char** nombre_out, char** tag_out);

#endif
