#ifndef MASTER_MAIN_
#define MASTER_MAIN_

#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

t_log *logger;
t_config *config;
t_log_level current_log_level;
char * puerto_query;

int main(int argc, char* argv[]);
void levantarConfig();
void *server_mh_query(void *args);

#endif