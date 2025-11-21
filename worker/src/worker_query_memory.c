#include <worker_query_memory.h>


void init_paginas(){
    cant_paginas = Tam_memoria / tam_pagina;
    Mem_paginas= malloc(sizeof(t_pagina) * cant_paginas);

    for (int i = 0; i < cant_paginas; i++){
        Mem_paginas[i].libre = true;
        Mem_paginas[i].presente = false;
        Mem_paginas[i].time = tiempo;
    }
}

void remplazar_pagina(char * tag){
    if (Especio_pagina() != -1){
       ocuapar_espacio(Especio_pagina(),tag);
    } else {
       proxima_victima(tag);
    }  
};


void ocuapar_espacio(int victima,char * tag){
    if (Mem_paginas[victima].libre == false){


    }else{
        Mem_paginas[victima].libre = false;
        Mem_paginas[victima].tag = tag;
        Mem_paginas[victima].presente = true;
    
        Mem_paginas[victima].modificado = false;
        Mem_paginas[victima].uso = true;
    
    }
    Mem_paginas[victima].time = tiempo;
    tiempo ++;

};

void proxima_victima(char * tag){
    if (strcmp(Algorit_Remplazo,"LRU")) ocuapar_espacio(Lru(),tag);
    
    if (strcmp(Algorit_Remplazo,"CLOCK-M")) ocuapar_espacio(Clock_M(),tag);

}


int Especio_pagina(){ // Devuelve el espacio libre
    for (int i = 0; i < cant_paginas; i++){
        if (Mem_paginas[i].libre == true){
            return i;
        }
    }
    return -1;
}


int Clock_M(){
    for (int i = 0; i < cant_paginas; i++){
        if (Mem_paginas[i].uso == false && Mem_paginas[i].modificado == false ){ // si es 0-0
           return i;
        }
    }
    for (int i = 0; i < cant_paginas; i++){
      if(Mem_paginas[i].uso == false && Mem_paginas[i].modificado == true){// si es 1-0
            return i;
        } else{
            Mem_paginas[i].uso = false;
        }
    }
    Clock_M();
};
int Lru(){ 
    int min = 0;
    for (int i = 0; i < cant_paginas; i++){
        if (Mem_paginas[i].time <= min){
            min = Mem_paginas[i].time;
        }  
    }
    return min;
};
