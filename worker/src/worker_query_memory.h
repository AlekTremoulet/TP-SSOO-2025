#ifndef WORKER_QUERY_MEM_
#define WORKER_QUERY_MEM_

#include <utils/utils.h>
#include <worker_query_interpreter.h>
#include <stdbool.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdint.h>

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
extern int socket_master;

void inicializar_memoria_interna(int tam_total, int tam_pagina);
void inicializar_paginas();


void remplazar_pagina(char * tag);
void liberar_memoria_interna();

int seleccionar_victima();


void proxima_victima(char * tag);
void ocuapar_espacio(int victima,char * tag);


qi_status_t ejercutar_WRITE_memoria(char * archivo,char * tag,int dir_base,char * contenido,int id_query);
qi_status_t ejecutar_READ_memoria(char* archivo, char* tag, int direccion_base, int tamanio, int query_id);
qi_status_t ejecutar_FLUSH_memoria(char* archivo, char* tag, int query_id);



#endif
