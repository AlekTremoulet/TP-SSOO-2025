#ifndef WORKER_MAIN_
#define WORKER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>


void inicializarWorker();
void levantarConfig(char* archivo_config);
void *conexion_cliente_master(void *args);

#endif
