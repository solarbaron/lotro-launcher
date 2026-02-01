/*
 * Socket interceptor for Wine/PatchClient.dll traffic capture
 * 
 * Usage:
 *   gcc -shared -fPIC -o socket_hook.so socket_hook.c -ldl
 *   LD_PRELOAD=./socket_hook.so wine rundll32.exe PatchClient.dll,Patch ...
 * 
 * This intercepts socket calls to capture the patch server protocol.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

// Target patch server
#define PATCH_SERVER_IP "64.37.188.7"
#define PATCH_SERVER_PORT 6015

// Log file
static FILE* log_file = NULL;
static int target_fd = -1;

// Original functions
static int (*real_connect)(int, const struct sockaddr*, socklen_t) = NULL;
static ssize_t (*real_send)(int, const void*, size_t, int) = NULL;
static ssize_t (*real_recv)(int, void*, size_t, int) = NULL;
static ssize_t (*real_write)(int, const void*, size_t) = NULL;
static ssize_t (*real_read)(int, void*, size_t) = NULL;

static void init_hooks(void) {
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        real_send = dlsym(RTLD_NEXT, "send");
        real_recv = dlsym(RTLD_NEXT, "recv");
        real_write = dlsym(RTLD_NEXT, "write");
        real_read = dlsym(RTLD_NEXT, "read");
    }
}

static void open_log(void) {
    if (!log_file) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/patch_traffic_%ld.log", (long)time(NULL));
        log_file = fopen(filename, "w");
        if (log_file) {
            fprintf(log_file, "=== Patch Traffic Capture ===\n");
            fprintf(log_file, "Target: %s:%d\n\n", PATCH_SERVER_IP, PATCH_SERVER_PORT);
            fflush(log_file);
            fprintf(stderr, "[HOOK] Logging to %s\n", filename);
        }
    }
}

static void log_hex(const char* prefix, const void* data, size_t len) {
    if (!log_file) return;
    
    const unsigned char* p = (const unsigned char*)data;
    fprintf(log_file, "%s (%zu bytes):\n", prefix, len);
    
    for (size_t i = 0; i < len && i < 1024; i++) {
        fprintf(log_file, "%02x ", p[i]);
        if ((i + 1) % 16 == 0) fprintf(log_file, "\n");
    }
    if (len > 1024) fprintf(log_file, "... (truncated)\n");
    fprintf(log_file, "\n\n");
    fflush(log_file);
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    init_hooks();
    
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* sin = (struct sockaddr_in*)addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
        int port = ntohs(sin->sin_port);
        
        if (strcmp(ip, PATCH_SERVER_IP) == 0 && port == PATCH_SERVER_PORT) {
            open_log();
            target_fd = sockfd;
            fprintf(stderr, "[HOOK] Intercepting connection to %s:%d (fd=%d)\n", ip, port, sockfd);
            if (log_file) {
                fprintf(log_file, "[CONNECT] %s:%d (fd=%d)\n\n", ip, port, sockfd);
                fflush(log_file);
            }
        }
    }
    
    return real_connect(sockfd, addr, addrlen);
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    init_hooks();
    
    if (sockfd == target_fd && log_file) {
        log_hex("[SEND]", buf, len);
    }
    
    return real_send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    init_hooks();
    
    ssize_t ret = real_recv(sockfd, buf, len, flags);
    
    if (sockfd == target_fd && ret > 0 && log_file) {
        log_hex("[RECV]", buf, ret);
    }
    
    return ret;
}

ssize_t write(int fd, const void* buf, size_t count) {
    init_hooks();
    
    if (fd == target_fd && log_file) {
        log_hex("[WRITE]", buf, count);
    }
    
    return real_write(fd, buf, count);
}

ssize_t read(int fd, void* buf, size_t count) {
    init_hooks();
    
    ssize_t ret = real_read(fd, buf, count);
    
    if (fd == target_fd && ret > 0 && log_file) {
        log_hex("[READ]", buf, ret);
    }
    
    return ret;
}
