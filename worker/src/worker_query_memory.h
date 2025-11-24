#ifndef WORKER_QUERY_MEM_
#define WORKER_QUERY_MEM_

#include <utils/utils.h>
#include <worker_query_interpreter.h>

#define PF

typedef struct {
    bool presente;
    bool uso;
    bool modificado;
    bool libre;
    int time;
} t_pagina;

typedef struct {
    char * tag;
    void * contenido;
    int cant_marcos;
    t_pagina * marcos;
} t_memoria;


void remplazar_pagina(char * tag);
void init_paginas();
void proxima_victima(char * tag);

void proxima_victima(char * tag);
void ocuapar_espacio(int victima,char * tag);
int Clock_M();
int Lru();
int Especio_pagina();
qi_status_t ejercutar_write_pag(char * archivo,char * tag,int dir_base,char * contenido,int id_query);

static bool memoria_tiene_paginas_necesarias(int direccion_base, int cantidad_bytes);
static void solicitar_paginas_a_storage(char *archivo, char *tag, int direccion_base, int cantidad_bytes);

extern int tam_pagina; //Del storage BLOCK_SIZE
t_pagina * Mem_paginas;
t_memoria * Memoria;
// int cant_paginas = 0;
int tiempo = 0; 

extern int Tam_memoria; // Del worker
extern int Retardo_reemplazo; // Del worker
extern char * Algorit_Remplazo; // Del worker
extern int socket_storage;
extern int socket_master;

#endif
