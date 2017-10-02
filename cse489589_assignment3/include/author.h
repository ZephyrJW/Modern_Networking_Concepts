#ifndef AUTHOR_H_
#define AUTHOR_H_

void author_response(int sock_index);

void init_response(int sock_index, int routerport, int dataport);

void routing_table_response(int sock_index);

void update_response(int sock_index, uint16_t targetid, uint16_t targetcost);

void crash_response(int sock_index);

void sendfile_response(int sock_index, uint32_t destination, uint8_t ttl, uint8_t transferid, uint16_t seqnumber, char * filename);

#endif
