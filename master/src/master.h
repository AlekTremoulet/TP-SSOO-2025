#ifndef MASTER_MAIN_
#define MASTER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
char * puerto;
extern int id_query_actual;

int main(int argc, char* argv[]);
void levantarConfig();
static void *handler_cliente(void *arg);
void* server_mh(void *args);


#endif