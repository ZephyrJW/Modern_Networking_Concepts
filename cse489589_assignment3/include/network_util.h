#ifndef NETWORK_UTIL_H_
#define NETWORK_UTIL_H_

#include <sys/stat.h>

ssize_t recvALL(int sock_index, char *buffer, ssize_t nbytes);
ssize_t sendALL(int sock_index, char *buffer, ssize_t nbytes);

//ssize_t UDP_sendAll(int sock_index, )

int getlocalIP(uint32_t *ret);

unsigned long get_filesize(const char *path);

#endif
