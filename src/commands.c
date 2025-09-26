#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include "commands.h"

int handle_command(RoomTable *RT, Client *cl, const char *cmdline, char *out, size_t outsz){
    // /list, /users, /leave
    if (strncmp(cmdline, "/list", 5)==0){
        char lst[512]; rooms_list(RT, lst, sizeof(lst));
        snprintf(out, outsz, "[SYS] salas: %s", lst);
        return 0;
    } else if (strncmp(cmdline, "/users", 6)==0){
        if (!cl->room[0]){ snprintf(out,outsz,"[ERR] NOT_IN_ROOM"); return -1; }
        Room *r = rooms_get(RT, cl->room);
        if (!r){ snprintf(out,outsz,"[ERR] ROOM_NOT_FOUND"); return -1; }
        char us[512]; room_users(r, us, sizeof(us));
        snprintf(out, outsz, "[SYS] usuarios en %s: %s", cl->room, us);
        return 0;
    } else if (strncmp(cmdline, "/leave", 6)==0){
        if (!cl->room[0]){ snprintf(out,outsz,"[ERR] NOT_IN_ROOM"); return -1; }
        Room *r = rooms_get(RT, cl->room);
        if (r){
            room_remove_member(r, cl);
            char note[256]; snprintf(note,sizeof(note),"[SYS] %s salió", cl->nick);
            room_enqueue(r, note, strlen(note));
        }
        char old[MAX_ROOM]; strncpy(old, cl->room, sizeof(old));
        cl->room[0]='\0';
        snprintf(out, outsz, "[OK] saliste de %s", old);
        return 0;
    }
    snprintf(out, outsz, "[ERR] UNKNOWN_CMD");
    return -1;
}
