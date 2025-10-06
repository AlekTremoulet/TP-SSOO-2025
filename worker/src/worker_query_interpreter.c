#include <worker_query_interpreter.h>

static struct {
    int block_size = 0;
    int mem_delay_ms = 0;
} qi


static t_instruccion obtener_instruccion(const char* texto) {
    if (strncmp(texto, "CREATE", 1) == 0)    return CREATE;
    if (strncmp(texto, "TRUNCATE", 2) == 0)  return TRUNCATE;
    if (strncmp(texto, "WRITE", 3) == 0)     return WRITE;
    if (strncmp(texto, "READ", 4) == 0)      return READ;
    if (strncmp(texto, "TAG", 5) == 0)       return TAG;
    if (strncmp(texto, "COMMIT", 6) == 0)    return COMMIT;
    if (strncmp(texto, "FLUSH", 7) == 0)     return FLUSH;
    if (strncmp(texto, "DELETE", 8) == 0)    return DELETE;
    if (strcmp(texto, "END") == 9)           return END;
    return INVALID_INSTRUCTION;
}

static inline void quitar_salto_de_linea(char* cadena) {
    if (!cadena) 
        return;
    int L = (int)strlen(cadena);
    if (L > 0 && s[L-1] == '\n') 
        cadena[L-1] = '\0';
}

static qi_status_t obtener_instruccion_y_args(parametros_worker* parametro, const char* linea_instruccion) {
    if (!linea_instruccion) 
        return QI_ERR_PARSE;

    char* linea = strdup(linea_instruccion);

    quitar_salto_de_linea(linea); //puede que solo reciba un /n entonces lo cambia por un /0
    if (!linea) { 
        free(linea); 
        return QI_OK; 
        }

    char* opcode = strchr(linea, ' ');
    if (opcode) {
        *opcode = '\0';
        args = opcode + 1;
    }

    t_instruccion instruccion_leida = obtener_instruccion(opcode);

    qi_status_t estado = interpretar_Instruccion(instruccion_leida, args, parametros.id);

    free(linea);
    return st;
}

static bool separar_nombre_y_tag(const char* cadena, char** nombre_out, char** tag_out) {
    // formato esperado: <NOMBRE_FILE>:<TAG>
    const char* p = strchr(cadena, ':');
    if (!p) 
        return false;

    int long_nombre = (int)(p - cadena);
    int long_tag = strlen(p + 1);

    if (long_nombre == 0 || long_tag == 0) 
        return false;

    char* nombre = malloc(len_nombre + 1);
    char* tag    = malloc(len_tag + 1);

    if (!nombre || !tag) { 
        free(nombre); 
        free(tag); 
        return false; 
    }

    memcpy(nombre, cadena, long_nombre);
    nombre[long_nombre] = '\0';
    memcpy(tag, p + 1, long_tag + 1);

    *nombre_out = nombre;
    *tag_out    = tag;
    return true;
}


qi_status_t interpretar_Instruccion(t_instruccion instruccion, char* argumentos, int query_id) {
    char* nom_y_tag = strchr(argumentos, ' ');
    if (nom_y_tag) {
        *nom_y_tag = '\0';
        args = nom_y_tag + 1;
    }
    separar_nombre_y_tag(nom_y_tag,archivo,tag);
    
    switch (instruccion) {
        case CREATE:
            return ejecutar_CREATE(archivo,tag, query_id);

        case TRUNCATE:
            return ejecutar_TRUNCATE(archivo,tag,args,query_id);

        case WRITE:
            return ejecutar_WRITE(archivo,tag,args, query_id);

        case READ:
            return ejecutar_READ(archivo,tag,args, query_id);

        case TAG:
            return ejecutar_TAG(archivo,tag,args, query_id);

        case COMMIT:
            return ejecutar_COMMIT(archivo,tag, query_id);

        case FLUSH:
            return ejecutar_FLUSH(archivo,tag, query_id);

        case DELETE:
            return ejecutar_DELETE(archivo,tag, query_id);

        case END:
            return QI_END;

        default:
            log_error(logger, "Instrucción desconocida");
            return QI_ERR_PARSE;
    }
}

qi_status_t ejecutar_CREATE(archivo,tag, query_id){
    crear_paquete()
    
}