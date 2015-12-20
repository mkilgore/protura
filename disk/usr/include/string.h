#ifndef _STRING_H
#define _STRING_H 1

#include <protura/types.h>

int    memcmp(const void *, const void *, size_t);
void  *memcpy(void *restrict, const void *restrict, size_t);
void  *memmove(void *, const void *, size_t);
void  *memset(void *, int, size_t);

char  *strcpy(char *restrict, const char *restrict);
char  *strcat(char *restrict, const char *restrict);
int    strcmp(const char *, const char *);
size_t strlen(const char *);
size_t strnlen(const char *, size_t len);
char *strncpy(char *restrict s1, const char *restrict s2, size_t len);
char *strncat(char *restrict s1, const char *restrict s2, size_t len);
int strncmp(const char *s1, const char *s2, size_t len);

#endif
