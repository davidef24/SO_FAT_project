#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //perror 
#include <string.h> //memset, memcpy ecc
#include <sys/mman.h> //mmap
#include <fcntl.h> //to open disk file
#include <unistd.h> // close, ftruncate
#include <stddef.h> // size_t


Wrapper* Fat_init(const char* filename){
    Wrapper * wrapper= (Wrapper*) malloc(sizeof(Wrapper));
    if (wrapper == NULL){
        return NULL;
    }
    wrapper->disk_fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (wrapper->disk_fd == -1){
        free(wrapper);
        perror("open error");
        return NULL;
    }
    if(ftruncate(wrapper->disk_fd, sizeof(Disk)) == -1){
        free(wrapper);
        perror("ftruncate error");
        return NULL;
    }		
    wrapper->current_disk = (Disk*) mmap(NULL, sizeof(Disk), PROT_READ | PROT_WRITE, MAP_SHARED, wrapper->disk_fd, 0);
    if (wrapper->current_disk == MAP_FAILED){
        perror("mmap error");
        free(wrapper);
        close(wrapper->disk_fd);
        return NULL;
    }
    //now we set current_dir to root directory, which has index 0 and name '/'

    wrapper->current_dir = 0;
    DirEntry entry = wrapper->current_disk->dir_table.entries[0];
    entry.entry_name[0] = '/';
    entry.entry_name[1] = '\0';
    
    //initialize fat table
    memset(&(wrapper->current_disk->fat_table), 0x00, sizeof(FatTable));
    return wrapper;
}

int Fat_destroy(Wrapper* wrapper){
    if(munmap(wrapper->current_disk, MMAPPED_MEMORY_SIZE) == -1){
        perror("unmap error");
        return -1;
    }
    if(close(wrapper->disk_fd) < 0){
        perror("close error");
        return -1;
    }
    free(wrapper);
    return 0;
}

//it will return a FileHanlde, but temporarily I put integer as return value
int createFile(const char* filename){
    return 0;
}

int eraseFile(FileHandle file){
    return 0;
}
int fat_write(FileHandle to, const void* in, size_t size) {
    return 0;
}
int fat_read(FileHandle from, void* out, size_t size) {
    return 0;
}
int fat_seek(FileHandle file, size_t offset, int whence){
    return 0;
}
int createDir(const char* dirName){
    return 0;
}
int eraseDir(const char* dirName){
    return 0;
}
int changeDir(const char* to){
    return 0;
}
int listDir(const char* dirName){
    return 0;
}