#ifndef QUERY_UTILITIES_
#define QUERY_UTILITIES_

#include <query.h>

void inicializarQuery();
void levantarConfig(char* archivo_config);
void *conexion_cliente_master(void *args);

#endif
