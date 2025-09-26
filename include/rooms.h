#ifndef ROOMS_H
#define ROOMS_H
#include "proto.h"

void rooms_init(RoomTable *T);
Room* rooms_get(RoomTable *T, const char *name);          // solo si existe
Room* rooms_get_or_create(RoomTable *T, const char *name);
void rooms_list(RoomTable *T, char *out, size_t outsz);

void room_start_worker(Room *r);
void room_stop_worker(Room *r);

void room_enqueue(Room *r, const char *line, size_t len);
int  room_add_member(Room *r, Client *cl);
void room_remove_member(Room *r, Client *cl);
void room_users(Room *r, char *out, size_t outsz);

#endif

