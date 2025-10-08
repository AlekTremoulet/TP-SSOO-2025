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

