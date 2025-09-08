#include <query.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
char *archivo_config;
char *archivo_query;
int prioridad;

int main(int argc, char* argv[]) {

    if (!argv[1] || !argv[2] || !argv[3]){
        printf("Error en los argumentos de la archivo");
        return EXIT_FAILURE;
    }
    
    archivo_config = argv[1];
    archivo_query = argv[2];
    prioridad = atoi(argv[3]);
    
    levantarConfig(archivo_config);
    inicializarQuery(archivo_query,prioridad);
    return 0;
}
