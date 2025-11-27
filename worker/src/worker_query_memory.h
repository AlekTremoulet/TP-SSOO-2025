#ifndef WORKER_QUERY_MEM_
#define WORKER_QUERY_MEM_

#include <utils/utils.h>
#include <worker_query_interpreter.h>

#define PF

typedef struct {
    bool presente;
    bool uso;
    bool modificado;
    int ult_usado; //para LRU
    int nro_pag_logica;
    bool libre;
    char * tag;
    char* archivo;
    void* data;
} t_pagina;

typedef struct {
    int tam_total;     
    int tam_pagina;
    int cant_marcos;
    t_pagina * marcos;
    int clock_puntero;      // puntero para CLOCK-M
    int time_count;    // contador lógico para LRU
} t_memoria;

t_pagina * Mem_paginas;
t_memoria * Memoria;


int tiempo = 0; 
extern int tam_pagina; //Del storage BLOCK_SIZE
extern int Tam_memoria; // Del worker
extern int Retardo_reemplazo; // Del worker
extern char * Algorit_Remplazo; // Del worker
extern int socket_storage;



void remplazar_pagina(char * tag);
void init_paginas();
void proxima_victima(char * tag);

void proxima_victima(char * tag);
void ocuapar_espacio(int victima,char * tag);
int Clock_M();
int Lru();
int Especio_pagina();
qi_status_t ejercutar_WRITE(char * archivo,char * tag,int dir_base,char * contenido,int id_query);
qi_status_t ejecutar_READ(char* archivo, char* tag, int direccion_base, int tamanio, int query_id);
qi_status_t ejecutar_FLUSH(char* archivo, char* tag, int query_id);

static bool memoria_tiene_paginas_necesarias(int direccion_base, int cantidad_bytes);
static void solicitar_paginas_a_storage(char *archivo, char *tag, int direccion_base, int cantidad_bytes);



#endif
