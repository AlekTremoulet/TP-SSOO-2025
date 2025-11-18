#include "operaciones.h"


void Crear_file(char* archivo,char* tag){
    crear_archivo_en_FS(archivo, tag);
}; 

void Truncar_file(char* archivo, char* tag, int tamanio, int query_id){
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config",dir_files,archivo,tag);
    t_config *config_a_truncar = config_create(metadata_config_asociado);
    config_set_value(config_a_truncar,"Tamanio",string_itoa(tamanio));
    config_save(config_a_truncar);
    log_debug(logger,metadata_config_asociado);
}; 
void Escrbir_bloque(char* archivo, char* tag, int dir_base, char* contenido, int query_id){
    
}; 
void Leer_bloque(char* archivo, char* tag, int dir_base, int tamanio, int query_id){

}; 

void Copiar_tag(char * Origen,char * Destino,char* tag_origen,char* tag_destino){
    //falta poner el work in progress y que copie el tag no el archivo completo
    char * comando = malloc(strlen(Origen) + strlen(Destino)+1 + 10);
    sprintf(comando ,"cp -r %s %s", Origen,Destino);
    // log_info(logger, "Copiar_tag");
    system(comando);
    free(comando);
};

void Eliminar_tag(char * Origen, char* tag, int query_id){
    //falta ver el tag
    
    char * comando = malloc(strlen(dir_files) + strlen(Origen) + strlen(tag) + 1 + 10);
    comando = "%s/%s/%s",dir_files,Origen,tag;
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
void Commit_tag(char* archivo, char* tag, int query_id){

}; 
