static struct {
    int block_size = 0;
    int mem_delay_ms = 0;
} qi

/*static void limpiar_salto(char* cadena) {
    if (!cadena) 
        return;
    int L = (int)strlen(cadena);
    if (L > 0 && cadena[L-1] == '\n') {
        cadena[L-1] = '\0';   // borra solo el salto de línea
    }
}
*/

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

static qi_status_t exec_CREATE(int qid, char* cadena) {
    // CREATE file:tag  -> el tamaño inicial del tag es 0 (lo define Storage)
    char *nombre_out = NULL, *tag_out = NULL;
    if (!separar_nombre_y_tag(cadena, &nombre_out, &tag_out)) 
        return QI_ERR_PARSE;


}