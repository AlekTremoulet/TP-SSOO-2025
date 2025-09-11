#include <query.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
char *archivo_config;
char *archivo_query;
int prioridad;
parametros_query parametros_a_enviar;

int main(int argc, char* argv[]) {

    if (argc < 4){
        printf("Error en los argumentos de la archivo");
        return EXIT_FAILURE;
    }
    
    archivo_config = argv[1];
    parametros_a_enviar.archivo = argv[2];
    parametros_a_enviar.prioridad = atoi(argv[3]);

    
    levantarConfig(archivo_config);
    inicializarQuery();
    return 0;
}
