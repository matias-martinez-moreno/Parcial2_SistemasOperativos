#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "persistence.h"

void ensure_data_dir(void){
    struct stat st;
    if (stat("data", &st) == -1) mkdir("data", 0700);
}

FILE* open_room_log(const char *roomname){
    ensure_data_dir();
    char path[256];
    snprintf(path, sizeof(path), "data/room_%s.log", roomname);
    return fopen(path, "a+");
}

void rotate_if_needed(FILE **pf, const char *roomname, long max_bytes){
    if (!*pf) return;
    long pos = ftell(*pf);
    if (pos >= max_bytes){
        fclose(*pf);
        char oldp[256], rotp[256];
        snprintf(oldp, sizeof(oldp), "data/room_%s.log", roomname);
        snprintf(rotp, sizeof(rotp), "data/room_%s.log.1", roomname);
        rename(oldp, rotp);
        *pf = open_room_log(roomname);
    }
}
