#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../include/proto.h"

typedef struct { int fd; } RxArgs;

static void* rx_thread(void *arg){
    RxArgs *a=(RxArgs*)arg;
    char buf[MAX_LINE+1];
    ssize_t n;
    while (1){
        int r = recv_frame(a->fd, buf, sizeof(buf), &n);
        if (r<0) break;
        puts(buf);
        fflush(stdout);
    }
    fprintf(stderr,"[INFO] desconectado del servidor\n");
    exit(0);
    return NULL;
}

int main(int argc, char **argv){
    if (argc<3){ fprintf(stderr,"Uso: %s <host> <puerto>\n", argv[0]); return 1; }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa={0}; sa.sin_family=AF_INET; sa.sin_port=htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &sa.sin_addr);
    if (connect(fd,(struct sockaddr*)&sa,sizeof(sa))<0){ perror("connect"); return 1; }

    pthread_t th; RxArgs a={fd};
    pthread_create(&th, NULL, rx_thread, &a);

    // Entrada interactiva: cada línea se envía como frame
    char line[MAX_LINE+1];
    while (fgets(line,sizeof(line),stdin)){
        size_t L = strcspn(line,"\n"); line[L]='\0';
        if (L==0) continue;
        send_frame(fd, line, strlen(line));
    }
    close(fd);
    return 0;
}
