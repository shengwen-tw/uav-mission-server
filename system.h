#ifndef __SYSTEM_H__
#define __SYSTEM_H__

void create_pidfile(int pid);
int read_pidfile();
void delete_pidfile();

#endif
