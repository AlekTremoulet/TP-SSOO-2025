#ifndef WORKER_QUERY_MEM_
#define WORKER_QUERY_MEM_

#include <utils/utils.h>

#define PF

typedef struct {
    char * tag;
    bool presente;
    bool uso;
    bool modificado;
    bool libre;
    int time;
} t_pagina;


void remplazar_pagina(char * tag);
void init_paginas();
void proxima_victima(char * tag);

void proxima_victima(char * tag);
void ocuapar_espacio(int victima,char * tag);
int Clock_M();
int Lru();
int Especio_pagina();

extern int tam_pagina; //Del storage BLOCK_SIZE
t_pagina * Mem_paginas;
int cant_paginas = 0;
int tiempo = 0; 

extern int Tam_memoria;
extern int Retardo_reemplazo;
extern char * Algorit_Remplazo;


#endif
