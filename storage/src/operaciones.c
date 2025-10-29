#include "operaciones.h"


void Copiar_tag(char * Origen,char * Destino){
    char * comando = malloc(strlen(Origen) + strlen(Destino)+1 + 10);


    sprintf(comando ,"cp -r %s %s", Origen,Destino);
    // log_info(logger, "Copiar_tag");
    system(comando);
    free(comando);
};
void Eliminar_tag(char * Origen){

    char * comando = malloc(strlen(Origen) + 1 + 10);

    DIR* dir = opendir("mydir");
    if (dir) {
        sprintf(comando ,"rm -rf %s %s", Origen); //Podría llegar a borrar todo
        closedir(dir);
    } else if (ENOENT == errno) {
       
    } else {
        
    }   
    system(comando);
    free(comando);
}; 
