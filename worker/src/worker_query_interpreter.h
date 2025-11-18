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
    QI_ERR_FILE, 
    QI_ERR_FUERA_LIMITE,
    QI_ERR_OP_NO_PERMITIDA, 
    QI_ERR_NO_ENCONTRADA, 
    QI_ERR_NO_HAY_ESPACIO
} qi_status_t;


static t_instruccion obtener_instruccion(const char* texto);
static inline void quitar_salto_de_linea(char* cadena);
static void liberar_string_split(char** array);
qi_status_t obtener_instruccion_y_args(void* parametros_worker, const char* linea, int query_id);
static bool separar_nombre_y_tag(const char* cadena, char** nombre_out, char** tag_out);
void ejecutar_query(const char* path_query, int query_id);
qi_status_t interpretar_Instruccion(t_instruccion instruccion, char** args, int query_id);
qi_status_t ejecutar_CREATE(char* archivo, char* tag, int query_id);
qi_status_t ejecutar_TRUNCATE(char* archivo, char* tag, int tamanio, int query_id);
qi_status_t ejecutar_WRITE(char* archivo, char* tag, int dir_base, char* contenido, int query_id);
qi_status_t ejecutar_READ(char* archivo, char* tag, int dir_base, int tamanio, int query_id);
qi_status_t ejecutar_TAG(char* arch_ori, char* tag_ori, char* arch_dest, char* tag_dest, int query_id);
qi_status_t ejecutar_COMMIT(char* archivo, char* tag, int query_id);
qi_status_t ejecutar_FLUSH(char* archivo, char* tag, int query_id);
qi_status_t ejecutar_DELETE(char* archivo, char* tag, int query_id);
static qi_status_t exec_CREATE(int qid, char* cadena);
static bool separar_nombre_y_tag(const char* cadena, char** nombre_out, char** tag_out);

void loop_principal();

#endif
