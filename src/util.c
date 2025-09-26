#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "proto.h"

static int writen(int fd, const void *buf, size_t n){
    const char *p = (const char*)buf;
    size_t left = n;
    while (left > 0){
        ssize_t w = write(fd, p, left);
        if (w < 0){
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        left -= (size_t)w; p += w;
    }
    return 0;
}

static int readn(int fd, void *buf, size_t n){
    char *p = (char*)buf;
    size_t left = n;
    while (left > 0){
        ssize_t r = read(fd, p, left);
        if (r < 0){
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1; // EOF
        left -= (size_t)r; p += r;
    }
    return 0;
}

int send_frame(int fd, const char *buf, size_t len){
    uint32_t be = htonl((uint32_t)len);
    if (writen(fd, &be, 4) < 0) return -1;
    if (writen(fd, buf, len) < 0) return -1;
    return 0;
}

int recv_frame(int fd, char *buf, size_t cap, ssize_t *out_len){
    uint32_t be = 0;
    if (readn(fd, &be, 4) < 0) return -1;
    uint32_t len = ntohl(be);
    if (len >= cap) return -2; // demasiado grande
    if (readn(fd, buf, len) < 0) return -1;
    buf[len] = '\0';
    if (out_len) *out_len = (ssize_t)len;
    return 0;
}
