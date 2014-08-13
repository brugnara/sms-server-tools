#ifndef HEADER_HELPERS_H
#define HEADER_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Used to select an appropriate header:
char *get_header(const char *header, char *header2);

// Return value: 1/0
// hlen = length of a header which matched.
int test_header(int *hlen, char *line, const char *header, char *header2);
int prepare_remove_headers(char *remove_headers, size_t size);

int change_headers(char *filename, char *remove_headers, char *add_buffer, char *add_headers, ...);

#endif
