#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void create_pidfile(int pid)
{
    FILE *fp = fopen(MASTER_PID_FILE, "w+");
    if (!fp) {
        printf("%s %s\n", strerror(errno), MASTER_PID_FILE);
        exit(0);
    }

    fprintf(fp, "%d", pid);
    fclose(fp);
}

int read_pidfile()
{
    char buff[32];
    FILE *fp = fopen(MASTER_PID_FILE, "r");
    if (!fp) {
        printf("Failed to open %s: %s\n", MASTER_PID_FILE, strerror(errno));
        exit(0);
    }

    if (!fgets(buff, sizeof(buff) - 1, fp)) {
        fclose(fp);
        exit(0);
    }

    fclose(fp);
    return atoi(buff);
}

void delete_pidfile()
{
    struct stat statfile;

    if (stat(MASTER_PID_FILE, &statfile) == 0)
        remove(MASTER_PID_FILE);
}
