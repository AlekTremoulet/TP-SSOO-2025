#include <worker_query_memory.h>

extern t_log* logger;
extern int tam_pagina;
extern int Tam_memoria;
extern char* Algorit_Remplazo;
extern int Retardo_reemplazo;
t_list* filetag_commiteados;
t_memoria* Memoria = NULL;

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
    for (int i = 0; i < Memoria->cant_marcos; i++) {
        if (Memoria->marcos[i].presente &&
            Memoria->marcos[i].nro_pag_logica == nro_pagina &&
            Memoria->marcos[i].archivo &&
            Memoria->marcos[i].tag &&
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

    // Primera pasada: buscar (uso=0, modificado=0)
    for (int i = 0; i < n; i++) {
        int victima = (Memoria->clock_puntero + i) % n;
        t_pagina* frame = &Memoria->marcos[victima];

        if (!frame->uso && !frame->modificado) {
            Memoria->clock_puntero = (victima + 1) % n;
            return victima;
        }
    }
    // Segunda pasada: buscar (uso=0, modificado=1) 

    for (int i = 0; i < n; i++) {
        int victima = (Memoria->clock_puntero + i) % n;
        t_pagina* frame = &Memoria->marcos[victima];

        if (!frame->uso && frame->modificado) {
            // muevo el puntero
            Memoria->clock_puntero = (victima + 1) % n;
            return victima;
        } else {
            // Los demás se les baja el uso
            frame->uso = false;
        }
    }

    // Si después de limpiar todos los bits de uso no encontramos víctima,
    // significa que al reiniciar el algoritmo ahora sí encontraremos alguna.
    return seleccionar_victima_CLOCK_M();
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
    else 
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


    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    protocolo_socket resp = recibir_paquete_ok(socket_storage);
    close(socket_storage);
    if (resp == OK) {
        frame->modificado = false;
        log_debug(logger, "Marco %d (p.%d %s:%s) enviado a Storage y marcado limpio",marco, frame->nro_pag_logica, frame->archivo, frame->tag);
        return true;
    } else {
        log_error(logger, "Error al enviar marco %d a Storage", marco);
        return false;
    }
}

static int cargar_pagina_en_marco(char* archivo, char* tag, int nro_pagina) {
    int existe = buscar_marco_por_pagina(archivo, tag, nro_pagina);
    if (existe != -1) 
        return existe;

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
        memset(Memoria->marcos[victima].data, 0, Memoria->tam_pagina);
        libre = victima;
    }

    int query_id_temp = obtener_query_id();

    t_paquete* paquete = crear_paquete(OP_READ);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &nro_pagina, sizeof(int));
    agregar_a_paquete(paquete, &query_id_temp, sizeof(int));

    int socket_storage = enviar_peticion_a_storage(paquete);
    eliminar_paquete(paquete);

    int size_recv = 0;
    protocolo_socket cod_op = recibir_operacion(socket_storage);
    t_list* paquete_recv = recibir_paquete(socket_storage);
    close(socket_storage);
    char * buffer = list_remove(paquete_recv,0);
    if (!buffer) {
        log_error(logger, "Error recibiendo pagina %d de %s:%s desde Storage", nro_pagina, archivo, tag);
        if (buffer) 
            free(buffer);
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
    if (hubo_COMMIT_no_se_puede_WRITE(archivo, tag)){
        log_error(logger, "WRITE rechazado: %s:%s ya tiene COMMIT", archivo, tag);
        return QI_ERR_COMMIT_CERRADO;
    }

    int longitud = (int)strlen(contenido)+1;
    log_info(logger, "## Query %d: WRITE en Memoria %s:%s desde %d (%d bytes)", obtener_query_id(), archivo, tag, dir_logica, longitud);

    if (dir_logica < 0 || longitud <= 0) {
        log_error(logger, "## Query %d: WRITE fuera de rango (dir %d len %d)", obtener_query_id(), dir_logica, longitud);
        return QI_ERR_PARSE;
    }

    if (!asegurar_paginas_cargadas(archivo, tag, dir_logica, longitud)) {
        log_error(logger, "## Query %d: Error al obtener páginas necesarias desde Storage", obtener_query_id());
        return QI_ERR_STORAGE;
    }

    // Escribir por partes en cada página
    int bytes_restantes = longitud;
    int cursor_logico = dir_logica;
    while (bytes_restantes > 0) {
        int p = direccion_a_pagina(cursor_logico);
        int offset = offset_en_pagina(cursor_logico);
        int marco = buscar_marco_por_pagina(archivo, tag, p);
        if (marco == -1) { 
            log_error(logger, "Inconsistencia: pagina no cargada tras asegurar"); 
            return QI_ERR_STORAGE; 
            }
        int espacio_en_pagina = Memoria->tam_pagina - offset;
        int a_escribir = (bytes_restantes <= espacio_en_pagina) ? bytes_restantes : espacio_en_pagina;
        memcpy((char*)Memoria->marcos[marco].data + offset, contenido + (longitud - bytes_restantes), a_escribir);


        Memoria->marcos[marco].modificado = true;
        Memoria->marcos[marco].uso = true;
        Memoria->marcos[marco].ult_usado = Memoria->time_count++;

        bytes_restantes -= a_escribir;
        cursor_logico += a_escribir;
    }

    log_info(logger, "## Query %d: WRITE completado en memoria local (%s:%s)", obtener_query_id(), archivo, tag);
    return QI_OK;
}

bool hubo_COMMIT_no_se_puede_WRITE(const char* archivo, const char* tag) {
    for (int i = 0; i < list_size(filetag_commiteados); i++) {
        t_commit* ft = list_get(filetag_commiteados, i);
        if (strcmp(ft->archivo, archivo) == 0 &&
            strcmp(ft->tag, tag) == 0) {
            return true;
        }
    }
    return false;
}

qi_status_t ejecutar_READ_memoria(char* archivo, char* tag, int direccion_base, int tamanio) {
    if (!Memoria) 
        return QI_ERR_FILE;
    if (!archivo || !tag || tamanio <= 0) 
        return QI_ERR_PARSE;

    log_info(logger, "## Query %d: READ en Memoria %s:%s desde %d (%d bytes)", obtener_query_id(), archivo, tag, direccion_base, tamanio);

    if (direccion_base < 0 || direccion_base + tamanio > Memoria->tam_total) {
        log_error(logger, "## Query %d: READ fuera de rango", obtener_query_id());
        return QI_ERR_PARSE;
    }

    if (!asegurar_paginas_cargadas(archivo, tag, direccion_base, tamanio)) {
        log_error(logger, "## Query %d: Error al obtener páginas necesarias desde Storage (READ)", obtener_query_id());
        return QI_ERR_STORAGE;
    }

    char* buffer = malloc(tamanio + 1);
    int bytes_restantes = tamanio;
    int cursor_logico = direccion_base;
    int pos_buf = 0;
    while (bytes_restantes > 0) {
        int p = direccion_a_pagina(cursor_logico);
        int offset = offset_en_pagina(cursor_logico);
        int marco = buscar_marco_por_pagina(archivo, tag, p);
        if (marco == -1) { 
            free(buffer); 
            log_error(logger, "Inconsistencia en READ"); 
            return QI_ERR_STORAGE; 
            }
        int espacio_en_pagina = Memoria->tam_pagina - offset;
        int a_leer = (bytes_restantes <= espacio_en_pagina) ? bytes_restantes : espacio_en_pagina;
        memcpy(buffer + pos_buf, (char*)Memoria->marcos[marco].data + offset, a_leer);

        Memoria->marcos[marco].uso = true;
        Memoria->marcos[marco].ult_usado = Memoria->time_count++;

        bytes_restantes -= a_leer;
        cursor_logico += a_leer;
        pos_buf += a_leer;
    }
    buffer[tamanio] = '\0';

    t_paquete* paquete = crear_paquete(OP_READ);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    // agregar_a_paquete(paquete, &direccion_base, sizeof(int));
    // agregar_a_paquete(paquete, &(query_id), sizeof(int));
    agregar_a_paquete(paquete, buffer, strlen(buffer)+1);

    enviar_paquete(paquete, socket_master);

    log_info(logger, "## Query %d: READ completado: '%s' (%s:%s)", obtener_query_id(), buffer, archivo, tag);
    free(buffer);
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
            // enviar marco i a Storage
            if (!enviar_marco_a_storage(i)) {
                log_error(logger, "## Query %d: Error enviando pagina %d durante FLUSH", obtener_query_id(), f->nro_pag_logica);
                return QI_ERR_STORAGE;
            }
            any_sent = true;
        }
    }

    if (any_sent) {
        log_info(logger, "## Query %d: FLUSH completado exitosamente (%s:%s)", obtener_query_id(), archivo, tag);
    } else {
        log_info(logger, "## Query %d: FLUSH: no habia paginas modificadas para %s:%s", obtener_query_id(), archivo, tag);
    }
    return QI_OK;
}

qi_status_t memoria_flush_global() { //cuando se desaloja limpia las paginas modificadas
    if (!Memoria)
        return QI_ERR_FILE;

    log_info(logger, "## Query %d: Ejecutando FLUSH GLOBAL", obtener_query_id());

    for (int i = 0; i < Memoria->cant_marcos; i++) {
        t_pagina* f = &Memoria->marcos[i];

        if (f->presente && f->modificado && f->archivo && f->tag) {
            if (!enviar_marco_a_storage(i)) {
                log_error(logger,"## Query %d: Error enviando página %d durante FLUSH GLOBAL",obtener_query_id(), f->nro_pag_logica);
                return QI_ERR_STORAGE;
            }
        }
    }
    log_info(logger, "## Query %d: FLUSH GLOBAL completado", obtener_query_id());
    return QI_OK;
}

