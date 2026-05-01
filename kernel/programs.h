#ifndef PROGRAMS_H
#define PROGRAMS_H

void init_programs();

void program_help();
void program_list();
void program_info(char *name);
int program_run(char *name, char *args);

#endif