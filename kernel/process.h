#ifndef PROCESS_H
#define PROCESS_H

#include "../cpu/types.h"

#define PROCESS_MAX 32

#define PROCESS_STATUS_EMPTY   0
#define PROCESS_STATUS_RUNNING 1
#define PROCESS_STATUS_SUCCESS 2
#define PROCESS_STATUS_FAILED  3

void init_processes();

int process_start(char *name, char *args);
void process_finish(int pid, int result);

void process_help();
void process_list();
void process_status();
void process_clear();

int process_count();
int process_get_current_pid();

#endif
