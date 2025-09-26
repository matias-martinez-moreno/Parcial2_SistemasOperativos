#ifndef COMMANDS_H
#define COMMANDS_H
#include "proto.h"
#include "rooms.h"

int handle_command(RoomTable *RT, Client *cl, const char *cmdline, char *out, size_t outsz);

#endif
