#include <worker_query_interpreter.h>

static struct {
    int block_size = 0;
    int mem_delay_ms = 0;
} qi


static t_instruccion obtener_instruccion(const char* texto) {
    if (strcmp(texto, "CREATE")   == 0) return CREATE;
    if (strcmp(texto, "TRUNCATE") == 0) return TRUNCATE;
    if (strcmp(texto, "WRITE")    == 0) return WRITE;
    if (strcmp(texto, "READ")     == 0) return READ;
    if (strcmp(texto, "TAG")      == 0) return TAG;
    if (strcmp(texto, "COMMIT")   == 0) return COMMIT;
    if (strcmp(texto, "FLUSH")    == 0) return FLUSH;
    if (strcmp(texto, "DELETE")   == 0) return DELETE;
    if (strcmp(texto, "END")      == 0) return END;
    return INVALID_INSTRUCTION;
}


static inline void quitar_salto_de_linea(char* cadena) {
    if (!cadena) 
        return;
    int L = (int)strlen(cadena);
    if (L > 0 && cadena[L-1] == '\n') 
        cadena[L-1] = '\0';
}

static void liberar_string_split(char** array) {
    for (int i = 0; array[i] != NULL; i++)
        free(array[i]);
    free(array);
}

qi_status_t obtener_instruccion_y_args(void* parametros_worker, const char* linea, int query_id) {
    if (!linea || strlen(linea) == 0)
        return QI_OK; //osea, si viene una linea vacia, que pase de largo. Tipo, ta bien

    char** partes = string_split(linea, " ");
    if (!partes || !partes[0]) {
        liberar_string_split(partes);
        return QI_ERR_PARSE;
    }

    t_instruccion instr = obtener_instruccion(partes[0]);
    qi_status_t status = interpretar_Instruccion(instr, partes, query_id);

    liberar_string_split(partes);
    return status;
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


qi_status_t interpretar_Instruccion(t_instruccion instruccion, char** args, int query_id) {
    char *archivo = NULL, *tag = NULL;
    qi_status_t result = QI_OK;

    switch (instruccion) {
        case CREATE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            if (!separar_nombre_y_tag(args[1], &archivo, &tag)) 
                return QI_ERR_PARSE;
            result = ejecutar_CREATE(archivo, tag, query_id);
            break;

        case TRUNCATE:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_TRUNCATE(archivo, tag, atoi(args[2]), query_id);
            break;

        case WRITE:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_WRITE(archivo, tag, atoi(args[2]), args[3], query_id);
            break;

        case READ:
            if (!args[1] || !args[2] || !args[3]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_READ(archivo, tag, atoi(args[2]), atoi(args[3]), query_id);
            break;

        case TAG:
            if (!args[1] || !args[2]) 
                return QI_ERR_PARSE;
            char *arch_ori, *tag_ori, *arch_dest, *tag_dest;
            separar_nombre_y_tag(args[1], &arch_ori, &tag_ori);
            separar_nombre_y_tag(args[2], &arch_dest, &tag_dest);
            result = ejecutar_TAG(arch_ori, tag_ori, arch_dest, tag_dest, query_id);
            free(arch_ori); 
            free(tag_ori); 
            free(arch_dest); 
            free(tag_dest);
            return result;

        case COMMIT:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_COMMIT(archivo, tag, query_id);
            break;

        case FLUSH:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_FLUSH(archivo, tag, query_id);
            break;

        case DELETE:
            if (!args[1]) 
                return QI_ERR_PARSE;
            separar_nombre_y_tag(args[1], &archivo, &tag);
            result = ejecutar_DELETE(archivo, tag, query_id);
            break;

        case END:
            return QI_END;

        default:
            log_error(logger, "Instrucción desconocida: %s", args[0]);
            result = QI_ERR_PARSE;
            break;
    }

    free(archivo);
    free(tag);
    return result;
}

void ejecutar_query(const char* path_query, int query_id) {
    FILE* arch_inst = fopen(path_query, "r");
    if (!arch_inst) {
        log_error(logger, "No se pudo abrir el archivo de Query: %s", path_query);
        return;
    }

    log_info(logger, "## Query %d: Se recibe la Query. El path de operaciones es: %s", query_id, path_query);

    char* linea = NULL;
    size_t len = 0;
    ssize_t read;
    int program_counter = 0;

    while ((read = getline(&linea, &len, arch_inst)) != -1) {
        quitar_salto_de_linea(linea);

        log_info(logger, "## Query %d: FETCH - Program Counter: %d - %s", query_id, program_counter, linea);

        qi_status_t st = obtener_instruccion_y_args(NULL, linea, query_id);

        log_info(logger, "## Query %d: - Instrucción realizada: %s", query_id, linea);

        if (st == QI_END) {
            log_info(logger, "## Query %d: Query finalizada correctamente", query_id);
            break;
        }

        if (st == QI_ERR_PARSE) {
            log_error(logger, "## Query %d: Error de parseo en línea %d", query_id, program_counter);
            break;
        }

        program_counter++;
    }

    free(linea);
    fclose(arch_inst);
}