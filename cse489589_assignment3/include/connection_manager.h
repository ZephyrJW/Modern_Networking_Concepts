#ifndef CONNECTION_MANAGER_H_
#define CONNECTION_MANAGER_H_

#include "../include/global.h"

int control_socket, router_socket, data_socket;
bool inited;

void init();

void updateDV();

#endif
