#include <master_query.h>

void *manager_query(void *args) {
    int server = iniciar_servidor(puerto);
    return 0;
}

char * ArchivoQueryInst(char * path_query){
    char * instrucciones;
    FILE * Archivo = fopen(path_query, "r+");
    int fileSize = 0;

    if (Archivo == NULL){
        log_error(logger, "Error al abrir el archivo de querys");
        exit(EXIT_FAILURE);
    }

    fseek (Archivo , 0 , SEEK_END);
    fileSize = ftell(Archivo);
    rewind (Archivo);
    instrucciones = (char*) malloc((sizeof(char)*fileSize));

    fread(instrucciones,fileSize,1,Archivo);
    fclose(Archivo);
    return instrucciones;
}

void query_enviar_lectura(query_t * q, char * archivo, char * tag, char * buffer){
    t_paquete * paquete_send = crear_paquete(QUERY_LECTURA);
    agregar_a_paquete(paquete_send, archivo, strlen(archivo)+1);
    agregar_a_paquete(paquete_send, tag, strlen(tag)+1);

    agregar_a_paquete(paquete_send, buffer, strlen(buffer)+1);

    enviar_paquete(paquete_send, q->socket_qc);
}
void query_finalizar(query_t * q, char* motivo){
    t_paquete * paquete_end = crear_paquete(QUERY_FINALIZACION);
    agregar_a_paquete(paquete_end, motivo, strlen(motivo)+1);
    enviar_paquete(paquete_end, q->socket_qc);
}

