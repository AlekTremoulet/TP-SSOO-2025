#include "operaciones.h"


void Crear_file(char* archivo,char* tag, int query_id){
    crear_archivo_en_FS(archivo, tag);
    log_info(logger,"<%d> - File Creado <%s>:<%s>",query_id,archivo,tag);
}; 

void Truncar_file(char* archivo, char* tag, int tamanio, int query_id){
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config",dir_files,archivo,tag);
    t_config *config_a_truncar = config_create(metadata_config_asociado);
    int tamanio_actual = config_get_int_value(config_a_truncar, "Tamanio");
    config_set_value(config_a_truncar,"Tamanio",string_itoa(tamanio));
    if (tamanio > tamanio_actual){
        //aumentar bloques
    } 
    else if (tamanio > tamanio_actual)
    {       
        //disminuir bloques
    }
    config_save(config_a_truncar);
    log_info(logger,"<%d> - File Truncado <%s>:<%s> - Tamaño: <%d>",query_id,archivo,tag,tamanio);
}; 
void Escrbir_bloque(char* archivo, char* tag, int dir_base, char* contenido, int query_id){
    int Siguiente_bit_libre = espacio_disponible(bitmap);
    if (Siguiente_bit_libre == -1 )
    {
        log_error(logger,"Error, Espacio Insuficiente");
    }
    else
    {
        buscar_y_ocupar_siguiente_bit_libre(Siguiente_bit_libre);
        char *nombre_archivo = malloc(20);
        sprintf(nombre_archivo, "/block%04d.dat", Siguiente_bit_libre);
        escribir_en_hash(nombre_archivo);
        log_info(logger,"<%d> - Bloque Lógico Escrito <%s>:<%s> - Número de Bloque: <%d>",query_id,archivo,tag,Siguiente_bit_libre);
    }

}; 

void Leer_bloque(char* archivo, char* tag, int dir_base, int tamanio, int query_id){
    log_info(logger,"<%d> - Bloque Lógico Leído <%s>:<%s> - Número de Bloque: <%d>",query_id,archivo,tag,tamanio);//aca no va tamanio si no que se saca el Numero de bloque
}; 

void Crear_tag(char * Origen,char * Destino,char* tag_origen,char* tag_destino, int query_id){
    //falta poner el work in progress y que copie el tag no el archivo completo
    char * comando = malloc(strlen(Origen) + strlen(Destino)+1 + 10);
    sprintf(comando ,"cp -r %s %s", Origen,Destino);
    system(comando);
    free(comando);
    log_info(logger,"<%d> - Tag Creado <%s>:<%s>",query_id,Destino,tag_destino);
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
    log_info(logger,"<%d> - Tag Eliminado <%s>:<%s>",query_id,Origen,tag);
}; 
void Commit_tag(char* archivo, char* tag, int query_id){
    log_info(logger,"<%d> - Commit de File:Tag <%s>:<%s>",query_id,archivo,tag);
}; 

