#ifndef UTILS_H_
#define UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <readline/readline.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>

//Conexiones
//Cliente -> Server
//QUERY -> MASTER
//WORKERS -> MASTER
//WORKERS -> STORAGE


typedef struct arg_struct {
    char * puerto;
    char * ip;
}argumentos_thread;

typedef struct list_struct{
    t_list *lista;
    pthread_mutex_t *mutex;
    sem_t *sem;
}list_struct_t;


enum protocolo_socket
{
    OK,
/*nuevas*/
    PARAMETROS_QUERY,
    QUERY_FINALIZACION,
    QUERY_LECTURA,
    EXEC_QUERY,
    DESALOJO,
    PARAMETROS_WORKER,
    DEVOLUCION_QUERY,
    WORKER_LECTURA,
    WORKER_FINALIZACION,
    WORKER_DESALOJO,
    PARAMETROS_STORAGE,
    OP_CREATE,
    OP_TRUNCATE,
    OP_WRITE,
    OP_READ,
    OP_TAG,
    OP_COMMIT,
    OP_FLUSH,
    OP_DELETE,
    OP_SOLICITAR_PAGINAS,
    OP_ESCRIBIR_PAGINA,
/*nuevas*/
    NOMBRE_IO,
    DORMIR_IO,
    PROCESS_CREATE_MEM,
    PROCESS_CREATE_MEM_FAIL,
    PROCESS_EXIT_MEM,
    SUSP_MEM,
    SUSP_MEM_ERROR,
    DUMP_MEM,
    DUMP_MEM_ERROR,
    UNSUSPEND_MEM,
    UNSUSPEND_MEM_ERROR,
    DESALOJO_CPU,
    DISPATCH_CPU,
    INTERRUPT_CPU,
    MOTIVO_DEVOLUCION_CPU,
    PROCESS_EXIT_CPU,
    PROCESS_INIT_CPU,
    DUMP_MEM_CPU,
    IO_CPU,
    READ_MEM,
    ACCEDER_A_TDP,
    DEVOLVER_MARCO, //para cuando se ejecuta ACCEDER_A_TDP (consultar, si es así esto)
    ACCEDER_A_ESPACIO_USUARIO,
    DEVOLVER_VALOR,
    LEER_PAG_COMPLETA,
    DEVOLVER_PAGINA,
    ACTUALIZAR_PAG_COMPLETA,
    PEDIR_INSTRUCCIONES,
    OBTENER_INSTRUCCION,
    DEVOLVER_INSTRUCCION,
    PEDIR_INSTRUCCION,
    ENVIAR_VALORES,
    REACTIVAR_SUSPENDIDOS,
};
typedef enum protocolo_socket protocolo_socket;

typedef enum {
    CREATE,
    TRUNCATE,
    WRITE,
    READ,
    TAG,
    COMMIT,
    FLUSH,
    DELETE,
    END,
    INVALID_INSTRUCTION
} t_instruccion;

typedef enum {
    READY_Q,
    EXECT_Q,
    EXIT_Q
} estados_query;

typedef enum {
    READY_w,
    EXECT_w,
    EXIT_w
} estados_worker;

typedef struct{
    char *archivo;
    int prioridad;
    int id_query;
    estados_query estado;
}parametros_query;

typedef struct{ 
    int id;
    }parametros_worker;


typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	protocolo_socket codigo_operacion;
	t_buffer* buffer;
} t_paquete;


extern t_log* logger;

//socket
    int iniciar_servidor(char *puerto);
    int esperar_cliente(int socket_servidor);
    int recibir_operacion(int socket_cliente);
    void *recibir_buffer(int *size, int socket_cliente);
    t_list* recibir_paquete(int socket_cliente);
    int recibir_paquete_ok(int socket_cliente);
    void *serializar_paquete(t_paquete *paquete, int bytes);
    int crear_conexion(char* ip, char* puerto);
    void crear_buffer(t_paquete* paquete);
    t_paquete* crear_paquete(protocolo_socket cod_op); 
    t_paquete* crear_paquete_ok(void); 
    void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
    void enviar_paquete(t_paquete* paquete, int socket_cliente);
    void enviar_paquete_ok(int socket_cliente);
    void eliminar_paquete(t_paquete *paquete);
    void liberar_conexion(int socket_cliente);
    
//socket
    void leer_consola(void);
    void terminar_programa(int conexion, t_log* logger, t_config* config);
    void iterator(char* value);
//

    list_struct_t * inicializarLista();

    sem_t *inicializarSem(int initial_value);

    pthread_cond_t *inicializarCond();

    pthread_mutex_t *inicializarMutex();
    void destrabar_flag_global(int *flag, pthread_mutex_t *mutex, pthread_cond_t *cond);
    void esperar_flag_global(int *flag, pthread_mutex_t *mutex, pthread_cond_t *cond);
    char* string_array_to_string(char** array);
#endif