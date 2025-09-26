#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "rooms.h"
#include "persistence.h"
#include "proto.h"

static void* room_worker(void *arg);

void rooms_init(RoomTable *T){
    memset(T, 0, sizeof(*T));
    pthread_rwlock_init(&T->rwlock, NULL);
}

static Room* room_new(const char *name){
    Room *r = (Room*)calloc(1, sizeof(Room));
    strncpy(r->name, name, sizeof(r->name)-1);
    pthread_mutex_init(&r->mtx, NULL);
    pthread_cond_init(&r->cv, NULL);
    r->mcap = 8;
    r->members = (Client**)calloc(r->mcap, sizeof(Client*));
    r->logf = open_room_log(name);
    r->running = 1;
    return r;
}

Room* rooms_get(RoomTable *T, const char *name){
    Room *ret = NULL;
    pthread_rwlock_rdlock(&T->rwlock);
    for (size_t i=0; i<T->count; i++){
        if (strcmp(T->arr[i]->name, name)==0){ ret = T->arr[i]; break; }
    }
    pthread_rwlock_unlock(&T->rwlock);
    return ret;
}

Room* rooms_get_or_create(RoomTable *T, const char *name){
    Room *r = rooms_get(T, name);
    if (r) return r;
    pthread_rwlock_wrlock(&T->rwlock);
    for (size_t i=0; i<T->count; i++){
        if (strcmp(T->arr[i]->name, name)==0){ pthread_rwlock_unlock(&T->rwlock); return T->arr[i]; }
    }
    if (T->count == T->cap){
        T->cap = T->cap? T->cap*2:8;
        T->arr = (Room**)realloc(T->arr, T->cap*sizeof(Room*));
    }
    r = room_new(name);
    T->arr[T->count++] = r;
    pthread_rwlock_unlock(&T->rwlock);
    room_start_worker(r);
    return r;
}

void rooms_list(RoomTable *T, char *out, size_t outsz){
    out[0]='\0';
    pthread_rwlock_rdlock(&T->rwlock);
    for(size_t i=0;i<T->count;i++){
        char tmp[96];
        snprintf(tmp,sizeof(tmp),"%s(%zu)%s", T->arr[i]->name, T->arr[i]->mcount, (i+1<T->count)?", ":"");
        strncat(out,tmp,outsz - strlen(out) - 1);
    }
    pthread_rwlock_unlock(&T->rwlock);
    if (out[0]=='\0') strncpy(out,"<sin salas>", outsz-1);
}

void room_start_worker(Room *r){
    pthread_create(&r->worker_tid, NULL, room_worker, r);
}

void room_stop_worker(Room *r){
    pthread_mutex_lock(&r->mtx);
    r->running = 0;
    pthread_cond_broadcast(&r->cv);
    pthread_mutex_unlock(&r->mtx);
    pthread_join(r->worker_tid, NULL);
}

void room_enqueue(Room *r, const char *line, size_t len){
    MsgNode *n = (MsgNode*)malloc(sizeof(MsgNode));
    n->data = (char*)malloc(len+1);
    memcpy(n->data, line, len);
    n->data[len]='\0';
    n->len = len; n->next = NULL;
    pthread_mutex_lock(&r->mtx);
    if (!r->tail) r->head = r->tail = n; else { r->tail->next = n; r->tail = n; }
    pthread_cond_signal(&r->cv);
    pthread_mutex_unlock(&r->mtx);
}

int room_add_member(Room *r, Client *cl){
    pthread_mutex_lock(&r->mtx);
    for (size_t i=0;i<r->mcount;i++) if (r->members[i]==cl){ pthread_mutex_unlock(&r->mtx); return 0; }
    if (r->mcount==r->mcap){ r->mcap*=2; r->members = (Client**)realloc(r->members, r->mcap*sizeof(Client*)); }
    r->members[r->mcount++] = cl;
    pthread_mutex_unlock(&r->mtx);
    return 0;
}

void room_remove_member(Room *r, Client *cl){
    pthread_mutex_lock(&r->mtx);
    for (size_t i=0;i<r->mcount;i++){
        if (r->members[i]==cl){ r->members[i]=r->members[--r->mcount]; break; }
    }
    pthread_mutex_unlock(&r->mtx);
}

void room_users(Room *r, char *out, size_t outsz){
    out[0]='\0';
    pthread_mutex_lock(&r->mtx);
    for (size_t i=0;i<r->mcount;i++){
        strncat(out, r->members[i]->nick, outsz - strlen(out) - 1);
        if (i+1<r->mcount) strncat(out, ", ", outsz - strlen(out) - 1);
    }
    pthread_mutex_unlock(&r->mtx);
    if (out[0]=='\0') strncpy(out,"<vacía>", outsz-1);
}

static void* room_worker(void *arg){
    Room *r = (Room*)arg;

    while (1){
        pthread_mutex_lock(&r->mtx);
        while (r->running && !r->head) pthread_cond_wait(&r->cv, &r->mtx);
        if (!r->running && !r->head){ pthread_mutex_unlock(&r->mtx); break; }
        MsgNode *n = r->head;
        r->head = n->next; if (!r->head) r->tail = NULL;
        size_t mc = r->mcount;
        Client **snap = (Client**)alloca(mc * sizeof(Client*));
        for (size_t i=0;i<mc;i++) snap[i]=r->members[i];
        pthread_mutex_unlock(&r->mtx);

        if (r->logf){
            fputs(n->data, r->logf); fputc('\n', r->logf); fflush(r->logf);
        }

        for (size_t i=0;i<mc;i++){
            if (snap[i]->fd > 0){
                send_frame(snap[i]->fd, n->data, n->len);
            }
        }
        free(n->data); free(n);
        rotate_if_needed(&r->logf, r->name, 10*1024*1024);
    }
    return NULL;
}
