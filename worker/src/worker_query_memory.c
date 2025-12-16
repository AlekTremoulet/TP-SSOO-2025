#include <worker_query_memory.h>

extern t_log* logger;
extern int tam_pagina;
extern int Tam_memoria;
extern char* Algorit_Remplazo;
extern int Retardo_reemplazo;
t_list* filetag_commiteados;
t_memoria* Memoria = NULL;

//workaround para no flushear 2 veces
int error_de_flush = false;

extern int socket_storage;

int obtener_query_id();

void inicializar_memoria_interna(int tam_total, int tam_pagina_local){
    if (Memoria) 
        return;

    Memoria = malloc(sizeof(t_memoria));
    Memoria->tam_total = tam_total;
    Memoria->tam_pagina = tam_pagina_local;
    Memoria->cant_marcos = tam_total / tam_pagina_local;
    Memoria->clock_puntero = 0;
    Memoria->time_count = 0;   
    filetag_commiteados = list_create(); 
    inicializar_paginas();
}

void inicializar_paginas(){
    Memoria->marcos = calloc(Memoria->cant_marcos, sizeof(t_pagina));

    for (int i = 0; i < Memoria->cant_marcos; i++) {
        Memoria->marcos[i].presente = false;
        Memoria->marcos[i].uso = false;
        Memoria->marcos[i].modificado = false;
        Memoria->marcos[i].ult_usado = 0;
        Memoria->marcos[i].nro_pag_logica = -1;
        Memoria->marcos[i].archivo = NULL;
        Memoria->marcos[i].tag = NULL;
        Memoria->marcos[i].data = calloc(1, Memoria->tam_pagina);
        memset(Memoria->marcos[i].data, 0, Memoria->tam_pagina);
    }

    log_info(logger, "Memoria inicializada: tam_total=%d tam_pagina=%d cant_marcos=%d",Memoria->tam_total, Memoria->tam_pagina, Memoria->cant_marcos);
}

void memoria_agregar_commit(const char* archivo, const char* tag) {
    for (int i = 0; i < list_size(filetag_commiteados); i++) {
        t_commit* ft = list_get(filetag_commiteados, i);
        if (strcmp(ft->archivo, archivo) == 0 && strcmp(ft->tag, tag) == 0)
            return; 
    }

    t_commit* nuevo = malloc(sizeof(t_commit));
    nuevo->archivo = strdup(archivo);
    nuevo->tag = strdup(tag);

    list_add(filetag_commiteados, nuevo);

    log_info(logger,"Memoria: agregando COMMIT para %s:%s", archivo, tag);
}

void liberar_memoria_interna() {
    if (!Memoria) 
        return;
    for (int i = 0; i < Memoria->cant_marcos; i++) {
        free(Memoria->marcos[i].data);
        free(Memoria->marcos[i].archivo);
        free(Memoria->marcos[i].tag);
    }
    free(Memoria->marcos);
    free(Memoria);
    Memoria = NULL;
}

void memoria_invalidar_paginas_fuera_de_rango(const char* archivo, const char* tag, int nuevo_tamanio) {
    int ultima_pag_valida = nuevo_tamanio / Memoria->tam_pagina;

    for (int i=0; i<Memoria->cant_marcos; i++) {
        t_pagina* p = &Memoria->marcos[i];
        if (p->presente &&
            strcmp(p->archivo, archivo)==0 &&
            strcmp(p->tag, tag)==0 &&
            p->nro_pag_logica >= ultima_pag_valida){
                p->presente = false;
            }
    }
}


static inline int direccion_a_pagina(int direccion) {
    return direccion / Memoria->tam_pagina;
}
static inline int offset_en_pagina(int direccion) {
    return direccion % Memoria->tam_pagina;
}



static int buscar_marco_por_pagina(const char* archivo, const char* tag, int nro_pagina) {
    log_debug(logger,"Buscando página %d de %s:%s",nro_pagina, archivo, tag);
    log_estado_marcos();
    for (int i = 0; i < Memoria->cant_marcos; i++) {

        if (Memoria->marcos[i].presente &&
            Memoria->marcos[i].nro_pag_logica == nro_pagina &&
            strcmp(Memoria->marcos[i].archivo, archivo) == 0 &&
            strcmp(Memoria->marcos[i].tag, tag) == 0) {
                return i;
        }
    }
    return -1;
}

static int buscar_marco_libre() {
    for (int i = 0; i < Memoria->cant_marcos; i++) {
        if (!Memoria->marcos[i].presente) 
            return i;
    }
    return -1;
}


static int seleccionar_victima_CLOCK_M() {
    int n = Memoria->cant_marcos;

    while (true) {
        t_pagina* f = &Memoria->marcos[Memoria->clock_puntero];

        if (!f->uso && !f->modificado) {
            int victima = Memoria->clock_puntero;
            Memoria->clock_puntero = (victima + 1) % n;
            return victima;
        }

        if (!f->uso && f->modificado) {
            int victima = Memoria->clock_puntero;
            Memoria->clock_puntero = (victima + 1) % n;
            return victima;
        }

        if (f->uso) {
            f->uso = false;
        }

        Memoria->clock_puntero = (Memoria->clock_puntero + 1) % n;
    }
}


static int seleccionar_victima_LRU() { 
    int min = 0;
    int min_timpo = Memoria->marcos[0].ult_usado;
    for (int i = 0; i < Memoria->cant_marcos; i++){
        if (Memoria->marcos[i].ult_usado < min_timpo){
            min_timpo = Memoria->marcos[i].ult_usado;
            min = i;
        }  
    }
    return min;
};

int seleccionar_victima(){
    int marco = 0;
    if (strcmp(Algorit_Remplazo, "LRU") == 0) 
        marco = seleccionar_victima_LRU();
    else if (strcmp(Algorit_Remplazo, "CLOCK-M") == 0) 
        marco = seleccionar_victima_CLOCK_M();
    return marco;
}

// escribe un marco en Storage si esta modificado 
static bool enviar_marco_a_storage(int marco) {
    t_pagina* frame = &Memoria->marcos[marco];
    if (!frame->modificado) 
        return true;
    if (!frame->archivo || !frame->tag) {
        log_error(logger, "Intentando flush de marco sin metadata");
        return false;
    }

    int query_id_temp = obtener_query_id();  

    t_paquete* paquete = crear_paquete(OP_WRITE);
    agregar_a_paquete(paquete, frame->archivo, strlen(frame->archivo) + 1);
    agregar_a_paquete(paquete, frame->tag, strlen(frame->tag) + 1);
    agregar_a_paquete(paquete, &frame->nro_pag_logica, sizeof(int));
    agregar_a_paquete(paquete, frame->data, Memoria->tam_pagina);
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    enviar_paquete(paquete, socket_storage);
    protocolo_socket resp = recibir_operacion(socket_storage);
    if (resp == OK) {
        t_list * paquete_recv = recibir_paquete(socket_storage);
        list_destroy_and_destroy_elements(paquete_recv, free);
        frame->modificado = false;
        log_debug(logger, "Marco %d (p.%d %s:%s) enviado a Storage y marcado limpio",marco, frame->nro_pag_logica, frame->archivo, frame->tag);
        return true;
    } else {
        log_error(logger, "Error al enviar marco %d a Storage", marco);
        t_list * paquete_recv = recibir_paquete(socket_storage);
        char * motivo = list_remove(paquete_recv, 0);
        enviar_error_a_master(resp,motivo);
        return false;
    }
}
static int cargar_pagina_en_marco(char* archivo, char* tag, int nro_pagina) {

    // buscar marco libre
    int libre = buscar_marco_libre();
    if (libre == -1) {
        // elegir victima y, si esta modificado, enviar a storage
        int victima = seleccionar_victima();
        // si victima está modificado -> WRITE de esa página concreta
        if (Memoria->marcos[victima].modificado) {
            if (!enviar_marco_a_storage(victima)) {
                log_error(logger, "No se pudo escribir la victima antes de reemplazar");
                return -1;
            }
        }
        // liberar metadata vieja
        free(Memoria->marcos[victima].archivo);
        free(Memoria->marcos[victima].tag);
        Memoria->marcos[victima].archivo = NULL;
        Memoria->marcos[victima].tag = NULL;
        Memoria->marcos[victima].presente = false;
        Memoria->marcos[victima].nro_pag_logica = -1;
        Memoria->marcos[victima].uso = false;
        Memoria->marcos[victima].modificado = false;
        //memset(Memoria->marcos[victima].data, 0, Memoria->tam_pagina);
        libre = victima;
    }

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_READ);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &nro_pagina, sizeof(int));
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    enviar_paquete(paquete, socket_storage);
    eliminar_paquete(paquete);

    int size_recv = 0;
    protocolo_socket cod_op = recibir_operacion(socket_storage);
    char * buffer;
    if(cod_op == OP_READ){
        t_list* paquete_recv = recibir_paquete(socket_storage);
        buffer = list_remove(paquete_recv,0);
        //list_destroy(paquete_recv);//no lo se rick
        if (!buffer) {
            log_error(logger, "Error recibiendo pagina %d de %s:%s desde Storage", nro_pagina, archivo, tag);
            if (buffer) 
                free(buffer);
            return -1;
        }
    }else {
        log_error(logger, "## Query %d: Error de Read", obtener_query_id());
        t_list * paquete_recv = recibir_paquete(socket_storage);
        char * motivo = list_remove(paquete_recv, 0);
        enviar_error_a_master(WORKER_FINALIZACION,motivo);
        //list_destroy(paquete_recv);
        return -1;
    }
   
    // Copiar al marco
    memcpy(Memoria->marcos[libre].data, buffer, Memoria->tam_pagina);
    free(buffer);

    // Setear metadata
    Memoria->marcos[libre].archivo = string_duplicate(archivo);
    Memoria->marcos[libre].tag = string_duplicate(tag);
    Memoria->marcos[libre].nro_pag_logica = nro_pagina;
    Memoria->marcos[libre].presente = true;
    Memoria->marcos[libre].uso = true;
    Memoria->marcos[libre].modificado = false;
    Memoria->marcos[libre].ult_usado = Memoria->time_count++;
    //Memoria->marcos[libre].libre = false;//no lo se rick
    log_debug(logger, "Pagina %d de %s:%s cargada en marco %d", nro_pagina, archivo, tag, libre);

    return libre;
}

static bool asegurar_paginas_cargadas(const char* archivo, const char* tag, int direccion_base, int cantidad_bytes) {
    if (cantidad_bytes <= 0) 
        return true;
    int pagina_ini = direccion_a_pagina(direccion_base);
    int pagina_fin = direccion_a_pagina(direccion_base + cantidad_bytes - 1);

    for (int p = pagina_ini; p <= pagina_fin; p++) {
        int existe = buscar_marco_por_pagina(archivo, tag, p);
        if (existe == -1) {
            existe = cargar_pagina_en_marco(archivo, tag, p);
            if (existe == QI_ERR_STORAGE){
                return false;
            }
            if (existe == -1) 
                return false;
            // hay que poner retardo?
            if (Retardo_reemplazo > 0) 
                usleep(Retardo_reemplazo * 1000);
        } else {
            Memoria->marcos[existe].uso = true;
            Memoria->marcos[existe].ult_usado = Memoria->time_count++;
        }
    }
    return true;
}

static int obtener_marco(const char* archivo,const char* tag,int nro_pagina,bool acciones_escritura) {
    int marco = buscar_marco_por_pagina(archivo, tag, nro_pagina);
    if (marco != -1) {
        Memoria->marcos[marco].uso = true;
        Memoria->marcos[marco].ult_usado = Memoria->time_count++;
         if (acciones_escritura) {
            Memoria->marcos[marco].modificado = true;
        }
        return marco;
    }

    marco = cargar_pagina_en_marco((char*)archivo, (char*)tag, nro_pagina);
    if (marco < 0)
        return -1;

    if (acciones_escritura) {
        Memoria->marcos[marco].modificado = true;
    }

    if (Retardo_reemplazo > 0)
        usleep(Retardo_reemplazo * 1000);
    return marco;
}
   
void memoria_actualizar_tag(char* arch_o, char* tag_o,char* arch_n, char* tag_n)
{
    for (int i=0;i<Memoria->cant_marcos;i++) {
        t_pagina* p = &Memoria->marcos[i];
        if (p->presente &&
            strcmp(p->archivo, arch_o)==0 &&
            strcmp(p->tag, tag_o)==0)
        {
            free(p->archivo);
            free(p->tag);
            p->archivo = string_duplicate(arch_n);
            p->tag    = string_duplicate(tag_n);
        }
    }
} 


void memoria_invalidar_file_tag_completo(const char* archivo, const char* tag) {
    memoria_eliminar_commit(archivo, tag);

    for (int i = 0; i < Memoria->cant_marcos; i++) {
        t_pagina* p = &Memoria->marcos[i];

        if (!p->presente) continue;
        if (!p->archivo || !p->tag) continue;

        if (strcmp(p->archivo, archivo) == 0 && strcmp(p->tag, tag) == 0) {
            free(p->archivo);
            free(p->tag);

            p->archivo       = NULL;
            p->tag           = NULL;
            p->presente      = false;
            p->modificado    = false;
            p->uso           = false;
            p->ult_usado     = 0;
            p->nro_pag_logica = -1;

            memset(p->data, 0, Memoria->tam_pagina);

        }
    }
    log_info(logger,"Memoria: invalidación total de %s:%s completada",archivo, tag);
}


void memoria_eliminar_commit(const char* archivo, const char* tag) {
    for (int i = 0; i < list_size(filetag_commiteados);) {
        t_commit* ft = list_get(filetag_commiteados, i);

        if (strcmp(ft->archivo, archivo) == 0 && strcmp(ft->tag, tag) == 0) {
            ft = list_remove(filetag_commiteados, i);

            free(ft->archivo);
            free(ft->tag);
            free(ft);
            continue; 
        }
        i++;
    }
    log_info(logger, "Memoria: eliminado COMMIT %s:%s (si existía)", archivo, tag);
}

void memoria_truncar(const char* archivo, const char* tag, int nuevo_tam)
{
    int tam_pag = Memoria->tam_pagina;
    int paginas_validas = (nuevo_tam + tam_pag - 1) / tam_pag;

    for (int i = 0; i < Memoria->cant_marcos; i++) {

        t_pagina* p = &Memoria->marcos[i];

        if (!p->archivo) continue;
        if (!p->archivo || !p->tag) continue;
        if (strcmp(p->archivo, archivo) != 0) continue;
        if (strcmp(p->tag, tag)!= 0) continue;

        // Si esta página está por encima del nuevo tamaño -> invalidar
        if (p->nro_pag_logica >= paginas_validas)
        {
            if (p->modificado)
            {
                if (!enviar_marco_a_storage(i))
                    log_error(logger, "Error al flushear marco %d durante TRUNCATE", i);
            }
            free(p->archivo);
            free(p->tag);
            p->archivo       = NULL;
            p->tag           = NULL;
            p->presente      = false;
            p->modificado    = false;
            p->uso           = false;
            p->ult_usado     = 0;
            p->nro_pag_logica = -1;
            memset(p->data, 0, Memoria->tam_pagina);
        }
    }
    log_info(logger, "Memoria: TRUNCATE finalizado para %s:%s, nuevo tamaño=%d",archivo, tag, nuevo_tam);
}

qi_status_t ejecutar_WRITE_memoria(char * archivo,char * tag,int dir_logica,char * contenido){
    if (!Memoria) 
        return QI_ERR_FILE;
    if (!archivo || !tag || !contenido) 
        return QI_ERR_PARSE;

    int longitud = (int)strlen(contenido)+1;
    log_info(logger, "## Query %d: WRITE en Memoria %s:%s desde %d (%d bytes)", obtener_query_id(), archivo, tag, dir_logica, longitud);

    if (dir_logica < 0 || longitud <= 0) {
        log_error(logger, "## Query %d: WRITE fuera de rango (dir %d len %d)", obtener_query_id(), dir_logica, longitud);
        return QI_ERR_PARSE;
    }

    int bytes_restantes = longitud;
    int cursor_logico = dir_logica;
    int pos_buf = 0;

    while (bytes_restantes > 0) {
        int pagina = direccion_a_pagina(cursor_logico);
        int offset = offset_en_pagina(cursor_logico);

        int marco = obtener_marco(archivo, tag, pagina, true);
        if (marco < 0) {
            log_error(logger, "Error obteniendo marco para WRITE");
            return QI_ERR_STORAGE;
        }

        int espacio = Memoria->tam_pagina - offset;
        int a_escribir = bytes_restantes < espacio ? bytes_restantes : espacio;

        memcpy((char*)Memoria->marcos[marco].data + offset,contenido + pos_buf,a_escribir);

        //Memoria->marcos[marco].modificado = true;
        //Memoria->marcos[marco].uso = true;
        //Memoria->marcos[marco].ult_usado = Memoria->time_count++;

        bytes_restantes -= a_escribir;
        cursor_logico += a_escribir;
        pos_buf += a_escribir;
    }

    log_info(logger,"## Query %d: WRITE completado (%s:%s)", obtener_query_id(), archivo, tag
    );
    log_estado_marcos();
    return QI_OK;
}


qi_status_t ejecutar_READ_memoria(char* archivo, char* tag, int dir_logica, int tamanio) {
    if (!Memoria) 
        return QI_ERR_FILE;
    if (!archivo || !tag || tamanio <= 0) 
        return QI_ERR_PARSE;

    log_info(logger, "## Query %d: READ en Memoria %s:%s desde %d (%d bytes)", obtener_query_id(), archivo, tag, dir_logica, tamanio);

    if (dir_logica < 0 || tamanio < 0) {
        log_error(logger, "## Query %d: READ fuera de rango", obtener_query_id());
        char * motivo = "READ fuera de rango";
        enviar_error_a_master(WORKER_FINALIZACION,motivo);
        return QI_ERR_PARSE;
    }

    //int longitud_real = ((tamanio + tam_pagina - 1) / tam_pagina) * tam_pagina;
    char* buffer = calloc(tamanio + 1, 1); 
    if (!buffer)
        return QI_ERR_STORAGE;

    int bytes_restantes = tamanio;
    int cursor_logico = dir_logica;
    int pos_buf = 0;

    while (bytes_restantes > 0) {
        int pagina = direccion_a_pagina(cursor_logico);
        int offset = offset_en_pagina(cursor_logico);

        int marco = obtener_marco(archivo, tag, pagina,false);
        if (marco < 0) {
            free(buffer);
            log_error(logger, "Error obteniendo marco para READ");
            return QI_ERR_STORAGE;
        }

        int espacio = Memoria->tam_pagina - offset;
        int a_leer = bytes_restantes <= espacio ? bytes_restantes : espacio;

        memcpy(buffer + pos_buf, (char*)Memoria->marcos[marco].data + offset, a_leer);

        //Memoria->marcos[marco].uso = true;
        //Memoria->marcos[marco].ult_usado = Memoria->time_count++;

        bytes_restantes -= a_leer;
        cursor_logico += a_leer;
        pos_buf += a_leer;

        int direccion_fisica = marco * Memoria->tam_pagina + offset;
        //char valor = ((char*)Memoria->marcos[marco].data)[offset];


        char* ptr_dato = (char*)Memoria->marcos[marco].data + offset;
       
        log_info(logger, 
            "Lectura Memoria: Query %d: Acción: LEER - Dirección Física: %d - Contenido leído: %.*s", 
            obtener_query_id(), direccion_fisica, a_leer,ptr_dato );

        //log_info(logger,
        //"Lectura Memoria: Query %d: Acción: LEER - Dirección Física: %d - Valor: %c", obtener_query_id(), 
        //direccion_fisica,valor);
    }
    

    t_paquete* paquete = crear_paquete(OP_READ);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, buffer, tamanio);

    enviar_paquete(paquete, socket_master);
    eliminar_paquete(paquete);

    free(buffer);
    log_estado_marcos();
    return QI_OK;
}

qi_status_t ejecutar_FLUSH_memoria(char* archivo, char* tag) {
    if (!Memoria) 
        return QI_ERR_FILE;
    if (!archivo || !tag) 
        return QI_ERR_PARSE;

    log_info(logger, "## Query %d: FLUSH iniciado para %s:%s", obtener_query_id(), archivo, tag);
    bool any_sent = false;
    for (int i = 0; i < Memoria->cant_marcos; i++) {
        t_pagina* f = &Memoria->marcos[i];
        if (f->presente && f->modificado && f->archivo && f->tag && strcmp(f->archivo, archivo) == 0 && strcmp(f->tag, tag) == 0) {
            if (!enviar_marco_a_storage(i)) {
                log_error(logger, "## Query %d: Error enviando pagina %d durante FLUSH", obtener_query_id(), f->nro_pag_logica);
                error_de_flush = true;
                return QI_ERR_FLUSH;
            }
            any_sent = true;
        }
    }
    log_estado_marcos();
    if (any_sent) {
        log_info(logger, "## Query %d: FLUSH completado exitosamente (%s:%s)", obtener_query_id(), archivo, tag);
    } else {
        log_info(logger, "## Query %d: FLUSH: no habia paginas modificadas para %s:%s", obtener_query_id(), archivo, tag);
    }
    return QI_OK;
}

qi_status_t memoria_flush_global(qi_status_t status) { //cuando se desaloja limpia las paginas modificadas

    if(status == QI_ERR_FLUSH){
        return QI_ERR_FLUSH;
    }
    
    if (!Memoria){
        return QI_ERR_FILE;
    }

    if(error_de_flush){
        return 0;
    }

    int id = obtener_query_id();

    log_info(logger, "## Query %d: Ejecutando FLUSH GLOBAL", id);
    for (int i = 0; i < Memoria->cant_marcos; i++) {
        t_pagina* f = &Memoria->marcos[i];

        if (f->presente && f->modificado && f->archivo && f->tag) {
            if (!enviar_marco_a_storage(i)) {
                log_error(logger,"## Query %d: Error enviando página %d durante FLUSH GLOBAL",id, f->nro_pag_logica);
                return QI_ERR_STORAGE;
            }
        }
    }
    log_info(logger, "## Query %d: FLUSH GLOBAL completado", id);
    return QI_OK;
}



void log_estado_marcos() {
    log_info(logger, "================= ESTADO DE MARCOS =================");
    log_info(logger, "Marco | File         | Tag | Página | Modificada | Uso | QueryID");
    log_info(logger, "---------------------------------------------------");

    for (int i = 0; i < Memoria->cant_marcos; i++) {
        t_pagina* m = &Memoria->marcos[i];

        log_info(logger,
            "%-5d | %-12s | %-3s | %-6d | %-10s | %-3d | %-7d",
            i,
            m->archivo ? m->archivo : "-",
            m->tag ? m->tag : "-",
            m->presente ? m->nro_pag_logica : "-",
            m->modificado ? "SI" : "NO",
            m->uso ? 1 : 0,
            obtener_query_id()
        );
    }

    log_info(logger, "===================================================");
}

