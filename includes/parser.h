#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

#define DELIM ": "
#define MAX_BUFFER_LEN 151

typedef struct _config{
	int workers;
	int mem;
	int files;
	char *sockname;
	char *log;
	bool tui;
	bool compression;
	unsigned short compression_level;
	unsigned char replacement_algo;
} config;

size_t strlen(const char *s);
int parse_config(FILE *conf, config *configuration);
void free_config(config *configuration);