#ifndef WORKER_QUERY_MEM_
#define WORKER_QUERY_MEM_

#include <utils/utils.h>

typedef struct {
    char * tag;
    bool presente;
    bool uso;
    bool modificado;
    bool libre;
    int time;
} t_pagina;


void remplazar_pagina();
void init_paginas();
void proxima_victima();

void proxima_victima();

void Clock_M();
void Lru();

int tam_pagina = 128; //Del storage BLOCK_SIZE
t_pagina * Mem_paginas[];
int cant_paginas = 0;
 
extern int Tam_memoria;
extern int Retardo_reemplazo;
extern char * Algorit_Remplazo;


#endif
