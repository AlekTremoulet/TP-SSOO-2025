#include <worker_query_memory.h>


void init_paginas(){
    cant_paginas = Tam_memoria / tam_pagina;
    * Mem_paginas= malloc(sizeof(t_pagina) * cant_paginas);
}

void remplazar_pagina(){
    if (Especio_pagina() != -1){
       
    } else {
       proxima_victima();
    }  
};

void proxima_victima(){
    if (strcmp(Algorit_Remplazo,"LRU")) Lru();
    
    if (strcmp(Algorit_Remplazo,"CLOCK-M")) Clock_M();

}


int Especio_pagina(){ // Devuelve el espacio libre
    for (int i = 0; i < cant_paginas; i++){
        if (Mem_paginas[i]->libre){
            return i;
        }
        
    }
    return -1;
}


void Clock_M(){

};
void Lru(){

};
