#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "proto.h"
#include "rooms.h"
#include "commands.h"

#define MAX_CLIENTS 512

static volatile int running = 1;
static void on_sigint(int s){ (void)s; running = 0; }

static int make_server_sock(uint16_t port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd,(struct sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); exit(1); }
    if (listen(fd,128)<0){ perror("listen"); exit(1); }
    return fd;
}

static void client_send_text(Client *cl, const char *txt){
    send_frame(cl->fd, txt, strlen(txt));
}

int main(int argc, char **argv){
    if (argc<2){ fprintf(stderr,"Uso: %s <puerto>\n", argv[0]); return 1; }
    signal(SIGINT, on_sigint);
    signal(SIGPIPE, SIG_IGN);

    int sfd = make_server_sock((uint16_t)atoi(argv[1]));
    printf("Servidor escuchando en puerto %s\n", argv[1]);

    RoomTable RT; rooms_init(&RT);
    Client clients[MAX_CLIENTS]; memset(clients,0,sizeof(clients));

    while (running){
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(sfd,&rfds);
        int maxfd = sfd;

        for (int i=0;i<MAX_CLIENTS;i++) if (clients[i].fd>0){
            FD_SET(clients[i].fd,&rfds);
            if (clients[i].fd>maxfd) maxfd=clients[i].fd;
        }

        struct timeval tv={1,0};
        int rv = select(maxfd+1,&rfds,NULL,NULL,&tv);
        if (rv<0){ if (errno==EINTR) continue; perror("select"); break; }

        if (FD_ISSET(sfd,&rfds)){
            int cfd = accept(sfd,NULL,NULL);
            if (cfd<0) continue;
            int idx=-1;
            for (int i=0;i<MAX_CLIENTS;i++) if (clients[i].fd<=0){ idx=i; break; }
            if (idx<0){ close(cfd); continue; }
            clients[idx].fd=cfd; clients[idx].nick[0]='\0'; clients[idx].room[0]='\0'; clients[idx].alive=1;
            client_send_text(&clients[idx], "[SYS] Bienvenido. Comandos: JOIN <sala> <nick> | MSG <texto> | CMD </list|/users|/leave>");
        }

        for (int i=0;i<MAX_CLIENTS;i++){
            Client *cl=&clients[i];
            if (cl->fd<=0) continue;
            if (!FD_ISSET(cl->fd,&rfds)) continue;

            char buf[MAX_LINE+1];
            ssize_t n=0;
            int r = recv_frame(cl->fd, buf, sizeof(buf), &n);
            if (r<0){
                if (cl->room[0]){
                    Room *r0 = rooms_get(&RT, cl->room);
                    if (r0){ room_remove_member(r0, cl);
                        char note[256]; snprintf(note,sizeof(note),"[SYS] %s desconectado", cl->nick[0]?cl->nick:"<anon>");
                        room_enqueue(r0, note, strlen(note));
                    }
                }
                close(cl->fd); memset(cl,0,sizeof(*cl));
                continue;
            }

            if (!strncmp(buf,"JOIN ",5)){
                char sala[MAX_ROOM]={0}, nick[MAX_NICK]={0};
                if (sscanf(buf+5, "%63s %31s", sala, nick)==2){
                    snprintf(cl->nick, sizeof(cl->nick), "%s", nick);
                    Room *r0 = rooms_get_or_create(&RT, sala);
                    if (cl->room[0]){
                        Room *prev = rooms_get(&RT, cl->room);
                        if (prev){ room_remove_member(prev, cl);
                            char note[256]; snprintf(note,sizeof(note),"[SYS] %s cambió a %s", cl->nick, sala);
                            room_enqueue(prev, note, strlen(note));
                        }
                    }
                    snprintf(cl->room, sizeof(cl->room), "%s", sala);
                    room_add_member(r0, cl);
                    char joinmsg[256]; snprintf(joinmsg,sizeof(joinmsg),"[SYS] %s se unió", cl->nick);
                    room_enqueue(r0, joinmsg, strlen(joinmsg));
                    client_send_text(cl, "[OK] JOIN");
                } else {
                    client_send_text(cl, "[ERR] Usa: JOIN <sala> <nick>");
                }
            } else if (!strncmp(buf,"MSG ",4)){
                if (!cl->room[0]){ client_send_text(cl,"[ERR] NOT_IN_ROOM"); continue; }
                Room *r0 = rooms_get(&RT, cl->room);
                if (!r0){ client_send_text(cl,"[ERR] ROOM_NOT_FOUND"); continue; }
                char line[MAX_LINE+64];
                snprintf(line,sizeof(line),"[%s@%s] %s", cl->nick[0]?cl->nick:"anon", cl->room, buf+4);
                room_enqueue(r0, line, strlen(line));
            } else if (!strncmp(buf,"CMD ",4)){
                char out[MAX_LINE+64];
                if (handle_command(&RT, cl, buf+4, out, sizeof(out))==0){
                    client_send_text(cl, out);
                } else {
                    client_send_text(cl, out);
                }
            } else {
                client_send_text(cl, "[ERR] Comando desconocido");
            }
        }
    }

    close(sfd);
    return 0;
}