#include "common_includes.h"
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif
void init_request(client_request* request, pid_t pid, unsigned char command, unsigned char flags, const char* pathname);
int serialize_request(client_request request, unsigned char** buffer, uint64_t* buffer_len);
int deserialize_request(client_request *request, unsigned char** buffer, uint64_t buffer_len);
int serialize_response(server_response response, unsigned char** buffer, uint64_t* buffer_len);
int deserialize_response(server_response *response, unsigned char** buffer, uint64_t buffer_len);
uint64_t char_to_ulong(unsigned char *array);
void ulong_to_char(uint64_t long_integer, unsigned char *array);
void reset_buffer(unsigned char** buffer, size_t* buff_size);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);
void clean_request(client_request* request);
void clean_response(server_response* response);