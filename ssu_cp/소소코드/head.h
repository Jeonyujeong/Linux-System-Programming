#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>

#define BUFFER_SIZE 1024

int isfile(char *fname);
void dirfork(char* source, char* target, int forkN);
void dircopy(char* source, char* target);
void filecopy(char* source, char* target);
void symcopy(char* source, char* target);
void option(int argc, char *argv[]);
void usage();
void infoCopy(char *source, char *target);
void infoPrint(char *source);

