#include <utils/utils.h>
#include "utils.h"


//socket
    int iniciar_servidor(char *puerto)
    {
        int socket_servidor;
        struct addrinfo hints, *servinfo;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        getaddrinfo(NULL, puerto, &hints, &servinfo);

        socket_servidor = socket(
            servinfo->ai_family,
            servinfo->ai_socktype,
            servinfo->ai_protocol);

        
        int opt = 1;
        setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
        listen(socket_servidor, SOMAXCONN);

        freeaddrinfo(servinfo);
        log_trace(logger, "Listo para escuchar a mi cliente");

        return socket_servidor;
    }

    int esperar_cliente(int socket_servidor)
    {
        int socket_cliente = accept(socket_servidor, NULL, NULL);
        log_debug(logger, "Se conecto un cliente!");

        return socket_cliente;
    }

    int recibir_operacion(int socket_cliente)
    {
        int cod_op;
        if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0) //function block hasta que llegue un paquete
            return cod_op;
        else
        {
            close(socket_cliente); //cierra la conexion si la conexion no existe
            return -1;
        }
    }

    void* recibir_buffer(int* size, int socket_cliente)
    {
        void * buffer;

        recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
        buffer = malloc(*size);
        recv(socket_cliente, buffer, *size, MSG_WAITALL);

        return buffer;
    }

    t_list* recibir_paquete(int socket_cliente)
    {
        int size;
        int desplazamiento = 0;
        void * buffer;
        t_list* valores = list_create();
        int tamanio;

        buffer = recibir_buffer(&size, socket_cliente);
        while(desplazamiento < size)
        {
            memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
            desplazamiento+=sizeof(int);
            char* valor = malloc(tamanio);
            memcpy(valor, buffer+desplazamiento, tamanio);
            desplazamiento+=tamanio;
            list_add(valores, valor);
        }
        free(buffer);
        return valores;
    }


    /// @brief Recibe un paquete de OK, en casos donde no se envie informacion valiosa. Limpia la lista para evitar errores
    /// @param socket_cliente 
    /// @return 0->ok, cod_op si es distinto de OK o se cerro la conexion
    int recibir_paquete_ok(int socket_cliente)
    { 
        protocolo_socket cod_op = recibir_operacion(socket_cliente);
        
        if(cod_op == -1){
            log_error(logger, "Conexión cerrada (cod_op = %d)", cod_op);
            return cod_op;
        }

        if (cod_op != OK) {
            log_debug(logger, "Se recibió cod_op distinto de OK (cod_op = %d)", cod_op);

            // consumir y liberar si aún así llega un paquete con basura
            t_list* basura = recibir_paquete(socket_cliente);
            if (basura != NULL) {
                list_destroy_and_destroy_elements(basura, free);
            }

            return cod_op;
        }

        // Si es OK, limpiamos los valores
        t_list* valores = recibir_paquete(socket_cliente);
        if (valores == NULL) {
            log_warning(logger, "No se recibieron valores tras OK, lista vacía o error en paquete");
            return cod_op; // sigue siendo 0 (OK), pero se loguea
        }

        list_destroy_and_destroy_elements(valores, free);

        return cod_op;
    }

    void* serializar_paquete(t_paquete* paquete, int bytes)
    {
        void * magic = malloc(bytes);
        int desplazamiento = 0;

        memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
        desplazamiento+= sizeof(int);
        memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
        desplazamiento+= sizeof(int);
        memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
        desplazamiento+= paquete->buffer->size;

        return magic;
    }

    int crear_conexion(char *ip, char* puerto)
    {
        struct addrinfo hints;
        struct addrinfo *server_info;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        getaddrinfo(ip, puerto, &hints, &server_info);

        int socket_cliente = socket(
                            server_info->ai_family,
                            server_info->ai_socktype,
                            server_info->ai_protocol);

        if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen)==-1){
            return -1;
        }else log_debug(logger, "Conectado al servidor");

        freeaddrinfo(server_info);

        return socket_cliente;
    }


    void crear_buffer(t_paquete* paquete)
    {
        paquete->buffer = malloc(sizeof(t_buffer));
        paquete->buffer->size = 0;
        paquete->buffer->stream = NULL;
    }

    t_paquete* crear_paquete(protocolo_socket cod_op)
    {
        t_paquete* paquete = malloc(sizeof(t_paquete));
        paquete->codigo_operacion = cod_op;
        crear_buffer(paquete);
        return paquete;
    }

    t_paquete* crear_paquete_ok()
    {
        t_paquete* paquete = malloc(sizeof(t_paquete));
        paquete->codigo_operacion = OK;
        crear_buffer(paquete);
        agregar_a_paquete(paquete, "OK", sizeof("OK"+1));
        return paquete;
    }

    void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
    {
        paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

        memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
        memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

        paquete->buffer->size += tamanio + sizeof(int);
    }

    void enviar_paquete(t_paquete* paquete, int socket_cliente)
    {
        int bytes = paquete->buffer->size + 2*sizeof(int);
        void* a_enviar = serializar_paquete(paquete, bytes);

        send(socket_cliente, a_enviar, bytes, 0);

        free(a_enviar);
    }

    /// @brief Maneja todo lo necesario para enviar un paquete de OK. Solo se necesita llamar a la funcion con el socket para que se cree y envie
    /// @param socket_cliente 
    void enviar_paquete_ok(int socket_cliente)
    {
        t_paquete * paquete;
        paquete = crear_paquete_ok(); // crea un paquete, le inserta ok y lo envia por socket
        int bytes = paquete->buffer->size + 2*sizeof(int);
        void* a_enviar = serializar_paquete(paquete, bytes);

        send(socket_cliente, a_enviar, bytes, 0);

        free(a_enviar);
        eliminar_paquete(paquete);
    }

    void eliminar_paquete(t_paquete* paquete)
    {
        free(paquete->buffer->stream);
        free(paquete->buffer);
        free(paquete);
    }

    void liberar_conexion(int socket_cliente)
    {
        close(socket_cliente);
    }


//socket

void leer_consola()
{
	char* leido;
	while(!string_is_empty(leido = readline("> "))){
        log_debug(logger, "%s", leido);
	}
	free(leido);
}


void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	close(conexion);
	config_destroy(config);

}

void iterator(char* value) {
	log_debug(logger,"%s", value);
}

list_struct_t * inicializarLista(){

    list_struct_t *lista = malloc(sizeof(list_struct_t));
    lista->lista = list_create();

    lista->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lista->mutex, NULL);

    lista->sem = inicializarSem(0);

    //log_debug(logger, "Se creo un list_struct nuevo");
    return lista;

}
sem_t *inicializarSem(int initial_value){
    sem_t * semaforo = malloc(sizeof(sem_t));
    sem_init(semaforo, 0, initial_value);
    return semaforo;
}
pthread_cond_t *inicializarCond(){
    pthread_cond_t * semaforo = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(semaforo, NULL);
    return semaforo;
}
pthread_mutex_t *inicializarMutex(){
    pthread_mutex_t * semaforo = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(semaforo, NULL);
    return semaforo;
}
void esperar_flag_global(int * flag, pthread_mutex_t *mutex, pthread_cond_t *cond){
    
    pthread_mutex_lock(mutex);
    while (!*flag) {
        pthread_cond_wait(cond, mutex);
    }
    pthread_mutex_unlock(mutex);

}

void destrabar_flag_global(int *flag, pthread_mutex_t *mutex, pthread_cond_t *cond){
    
    pthread_mutex_lock(mutex);
    *flag = 1;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);
}