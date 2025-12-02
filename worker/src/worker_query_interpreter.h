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
    QI_ERR_NO_HAY_ESPACIO,
    QI_ERR_STORAGE,
    QI_ERR_COMMIT_CERRADO
} qi_status_t;

extern int flag_desalojo;
extern pthread_mutex_t * mutex_flag_desalojo;




qi_status_t obtener_instruccion_y_args(void* parametros_worker,  char* linea, int query_id);
qi_status_t interpretar_Instruccion(t_instruccion instruccion, char** args, int query_id);

int obtener_desalojo_flag();
void setear_desalojo_flag(int value);
void loop_principal();

char *obtener_instruccion_index(list_struct_t *lista_queries, int PC);

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

int enviar_peticion_a_storage(t_paquete *paquete);

void loop_principal();

int obtener_desalojo_flag();
void setear_desalojo_flag(int value);

#endif
