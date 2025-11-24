#include <worker_query_memory.h>


void init_paginas(){
    Memoria = malloc(sizeof(t_memoria));
    Memoria->cant_marcos = Tam_memoria / tam_pagina;

    Memoria->marcos = malloc(sizeof(t_pagina) * Memoria->cant_marcos);

    for (int i = 0; i < Memoria->cant_marcos; i++){
        Memoria->marcos[i].libre = true;
        Memoria->marcos[i].presente = false;
        Memoria->marcos[i].time = tiempo;
    }
    log_debug(logger, "Parametros Storage: Block size <%d> Cant de paginas <%d> Tam memoria <%d>",tam_pagina,Memoria->cant_marcos,Tam_memoria);
}

void remplazar_pagina(char * tag){
    if (Especio_pagina() != -1){
       ocuapar_espacio(Especio_pagina(),tag);
    } else {
       proxima_victima(tag);
    }  
};


void ocuapar_espacio(int victima,char * tag){
    if (Memoria->marcos[victima].libre == false){ //Si esta ocupado el marco
        
    }else{
        Memoria->marcos[victima].libre = false;
        // Memoria->marcos[victima].tag = tag;
        Memoria->marcos[victima].presente = true;
    
        Memoria->marcos[victima].modificado = false;
        Memoria->marcos[victima].uso = true;
    
    }
    Memoria->marcos[victima].time = tiempo;
    tiempo ++;

};

void proxima_victima(char * tag){
    if (strcmp(Algorit_Remplazo,"LRU")) ocuapar_espacio(Lru(),tag);
    
    if (strcmp(Algorit_Remplazo,"CLOCK-M")) ocuapar_espacio(Clock_M(),tag);

}


int Especio_pagina(){ // Devuelve el espacio libre
    for (int i = 0; i < Memoria->cant_marcos; i++){
        if (Memoria->marcos[i].libre == true){
            return i;
        }
    }
    return -1;
}

int Cant_Especio_libre_pagina(){ // Devuelve el espacio libre
    int libre = 0;
    for (int i = 0; i < Memoria->cant_marcos; i++){
        if (Memoria->marcos[i].libre == true){
            libre++;
        }
    }
    return libre;
}


int Clock_M(){
    for (int i = 0; i < Memoria->cant_marcos; i++){
        if (Memoria->marcos[i].uso == false && Memoria->marcos[i].modificado == false ){ // si es 0-0
           return i;
        }
    }
    for (int i = 0; i < Memoria->cant_marcos; i++){
      if(Memoria->marcos[i].uso == false && Memoria->marcos[i].modificado == true){// si es 1-0
            return i;
        } else{
            Memoria->marcos[i].uso = false;
        }
    }
    return Clock_M();
};
int Lru(){ 
    int min = 0;
    for (int i = 0; i < Memoria->cant_marcos; i++){
        if (Memoria->marcos[i].time <= min){
            min = Memoria->marcos[i].time;
        }  
    }
    return min;
};


qi_status_t ejercutar_write_pag(char * archivo,char * tag,int dir_base,char * contenido,int id_query){
    int longitud = strlen(contenido) +1;
    if(!memoria_tiene_paginas_necesarias(dir_base,  longitud)) {
        solicitar_paginas_a_storage(archivo,tag,dir_base,longitud);
    }

    memcpy(Memoria->contenido + dir_base, contenido, longitud);

    return QI_OK;
}

static bool memoria_tiene_paginas_necesarias(int direccion_base, int cantidad_bytes) {
    int pagina_inicial = direccion_base / Memoria->cant_marcos;
    int pagina_final = (direccion_base + cantidad_bytes - 1) / Memoria->cant_marcos;

    for (int i = pagina_inicial; i <= pagina_final; i++) {
        if (i >= Memoria->cant_marcos)return false;
    }
    return true;
}

static void solicitar_paginas_a_storage(char* archivo, char* tag, int direccion_base, int cantidad_bytes) {
    log_info(logger, "Solicitando a Storage páginas faltantes para %s:%s", archivo, tag);

    t_paquete* paquete = crear_paquete(OP_SOLICITAR_PAGINAS);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &direccion_base, sizeof(int));
    agregar_a_paquete(paquete, &cantidad_bytes, sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    protocolo_socket respuesta = recibir_paquete_ok(socket_storage);
    if (respuesta == OK)
        log_info(logger, "Páginas solicitadas recibidas correctamente");
    else
        log_error(logger, "Error al recibir páginas de Storage");
}
