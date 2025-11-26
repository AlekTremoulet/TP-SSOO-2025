#include "operaciones.h"


void Crear_file(char* archivo,char* tag, int query_id){
    crear_archivo_en_FS(archivo, tag);
    log_info(logger,"<%d> - File Creado <%s>:<%s>",query_id,archivo,tag);
}; 

void Truncar_file(char* archivo, char* tag, int tamanio, int query_id) 
{
    char * metadata_config_asociado = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 1 + 20);
    sprintf(metadata_config_asociado, "%s/%s/%s/metadata.config", dir_files, archivo, tag);

    t_config *config_a_truncar = config_create(metadata_config_asociado);
    int tamanio_actual = config_get_int_value(config_a_truncar, "Tamanio");
    config_set_value(config_a_truncar, "Tamanio", string_itoa(tamanio));

    char** bloques_actuales = config_get_array_value(config_a_truncar, "Blocks");
    char* lista_final = string_new();

    char* archivo_tag = malloc(strlen(dir_files) + strlen(archivo) + strlen(tag) + 5);
    sprintf(archivo_tag, "%s/%s/%s", dir_files, archivo, tag);

    string_append(&lista_final, "[");

    for (int i = 0; bloques_actuales[i] != NULL; i++) {
        if (i > 0) {
            string_append(&lista_final, ",");
        }
        string_append(&lista_final, bloques_actuales[i]);
    }

    if (tamanio > tamanio_actual) {
        int tamanio_diferencia = tamanio - tamanio_actual;
        for (int i = 0; i < tamanio_diferencia; i++) {
            int Siguiente_bit_libre = espacio_disponible(bitmap);
            
            if (Siguiente_bit_libre == -1) {
                log_error(logger, "Error, Espacio Insuficiente");
                break; 
            } 
            else {
                buscar_y_ocupar_siguiente_bit_libre(Siguiente_bit_libre);
                char *nombre_archivo = malloc(20);
                sprintf(nombre_archivo, "/block%04d", Siguiente_bit_libre);
                
                escribir_en_hash(nombre_archivo); 
                log_info(logger, "<%d> - Bloque Reservado: %d", query_id, Siguiente_bit_libre);

                if (strlen(lista_final) > 1) { 
                     string_append(&lista_final, ",");
                } 
                string_append_with_format(&lista_final, "%d", Siguiente_bit_libre);
                
                char* logical_blocks_archivo_tagi = malloc(strlen(archivo_tag) + 50);
                sprintf(logical_blocks_archivo_tagi, "%s/logical_blocks/%06d.dat", archivo_tag, Siguiente_bit_libre);
                char* bloque_archivo = malloc(strlen(archivo_tag) + strlen(nombre_archivo));
                sprintf(bloque_archivo, "%s%s.dat", dir_physical_blocks, nombre_archivo);
                link(bloque_archivo,logical_blocks_archivo_tagi);
                log_info(logger, "<%d> - <%s>:<%s> Se agregó el hard link del bloque lógico <%s> al bloque físico <%s>", query_id,archivo,tag,logical_blocks_archivo_tagi,bloque_archivo);
                free(logical_blocks_archivo_tagi);
                free(nombre_archivo); 
            }
        }
        
    } else if (tamanio < tamanio_actual) { 
        //falta la logica de desasignar
    }

    string_append(&lista_final, "]");
    config_set_value(config_a_truncar, "Blocks", lista_final);
    config_save(config_a_truncar);

    string_iterate_lines(bloques_actuales, (void*) free);
    free(bloques_actuales);
    free(lista_final);
    free(archivo_tag);
    config_destroy(config_a_truncar); 
    free(metadata_config_asociado);   

    log_info(logger, "<%d> - File Truncado <%s>:<%s> - Tamaño: <%d>", query_id, archivo, tag, tamanio);
}

void Escrbir_bloque(char* archivo, char* tag, int dir_base, char* contenido, int query_id){
    char* logical_blocks_archivo_tag;
    log_info(logger,"<%d> - Bloque Lógico Escrito <%s>:<%s> - Número de Bloque: <%d>",query_id,archivo,tag,dir_base);//aca no va dir_base si no que se saca el Numero de bloque
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
    t_estado_metadata estado_nuevo = WIP;
    char* ruta_archivo = malloc(strlen(dir_files) + strlen(Destino) + strlen(tag_destino) + 1);
    sprintf(ruta_archivo, "%s//",dir_files,Destino,tag_destino);
    Cambiar_estado(ruta_archivo, estado_nuevo);
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

void Cambiar_estado(char* ruta_archivo, t_estado_metadata estado){
    t_config *Config_a_cambiar = config_create(ruta_archivo);
    switch (estado)
    {
    case WIP:
        config_set_value(Config_a_cambiar, "ESTADO", "WORK IN PROGRESS");
        break;
    case COMMITED:
        config_set_value(Config_a_cambiar, "ESTADO", "COMMITED");
        break;
    default:
        log_error(logger,"ESTADO NO RECONOCIDO");
        break;
    }
    config_destroy(Config_a_cambiar);
}