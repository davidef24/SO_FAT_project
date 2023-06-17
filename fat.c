#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //perror 
#include <string.h> //memset, memcpy ecc
#include <sys/mman.h> //mmap
#include <fcntl.h> //to open disk file
#include <unistd.h> // close, ftruncate
#include <stddef.h> // size_t



FileHandle createFile(const char* filename){
    return;
}

int eraseFile(FileHandle file){
    return 0;
}
int write(FileHandle to, const void* in, size_t size) {
    return 0;
}
int read(FileHandle from, void* out, size_t size) {
    return 0;
}
int seek(FileHandle file, size_t offset, int whence){
    return 0;
}
int createDir(const char* dirname){
    return 0;
}
int eraseDir(const char* dirname){
    return 0;
}
int changeDir(const char* new_dirname){
    return 0;
}
int listDir(const char* dirname){
    return 0;
}