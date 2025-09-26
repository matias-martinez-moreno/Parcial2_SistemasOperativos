#ifndef PERSISTENCE_H
#define PERSISTENCE_H
#include <stdio.h>
#include <stddef.h>

FILE* open_room_log(const char *roomname);
void  rotate_if_needed(FILE **pf, const char *roomname, long max_bytes);
void  ensure_data_dir(void);

#endif
