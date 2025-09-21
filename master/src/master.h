#ifndef MASTER_MAIN_
#define MASTER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

extern t_log *logger;
extern t_config *config;
extern t_log_level current_log_level;
extern char * puerto;
extern int id_query_actual;

int main(int argc, char* argv[]);
void levantarConfig();
void* server_mh(void *args);

#endif