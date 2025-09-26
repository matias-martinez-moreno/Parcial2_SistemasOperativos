#ifndef PROTO_H
#define PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

#define MAX_NICK 32
#define MAX_ROOM 64
#define MAX_LINE 1024

typedef struct Client {
    int fd;
    char nick[MAX_NICK];
    char room[MAX_ROOM];   // vacío si no está en sala
    int alive;
} Client;

typedef struct MsgNode {
    size_t len;
    char *data;            // texto UTF-8 (una línea lógica)
    struct MsgNode *next;
} MsgNode;

typedef struct Room {
    char name[MAX_ROOM];
    MsgNode *head, *tail;                   // cola FIFO
    pthread_mutex_t mtx;
    pthread_cond_t  cv;

    Client **members;                        // array de punteros
    size_t mcount, mcap;

    FILE *logf;                              // persistencia
    pthread_t worker_tid;                    // hilo worker
    int running;
} Room;

typedef struct {
    Room **arr;
    size_t count, cap;
    pthread_rwlock_t rwlock; // lectura concurrente / escritura exclusiva
} RoomTable;

/* framing: 4 bytes len big-endian + payload */
int send_frame(int fd, const char *buf, size_t len);
int recv_frame(int fd, char *buf, size_t cap, ssize_t *out_len);

#endif
