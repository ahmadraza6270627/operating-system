#ifndef APPFS_H
#define APPFS_H

void init_appfs();

void appfs_help();
void appfs_status();
void appfs_info(char *filename);
int appfs_run(char *filename);

#endif
