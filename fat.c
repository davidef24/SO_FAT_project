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
    DirEntry* entry = &(wrapper->current_disk->dir_table.entries[0]);
    entry->entry_name[0] = '/';
    entry->entry_name[1] = '\0';
    
    //initialize fat table
    //for now, if an entry value is 0 this means that it is free
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

//result represents the index in the directory table entries array, if -1 there is an error 
int find_entry(Wrapper* wrapper){
    Disk * disk = wrapper->current_disk;
    int res = -1;
    
    //we skip root directory
    for(int i=0; i< DIRECTORY_ENTRIES_NUM; i++){
        DirEntry entry = disk->dir_table.entries[i];
        if(entry.entry_name[0] == 0){
            res = i;
            break;
        }
    }

    //if res is still -1, there are no free entries
    if(res == -1){
        perror("no free entries");
        return -1;
    }

    //check if current directory has reached max children number
    int current_directory_index = wrapper->current_dir;
    if(wrapper->current_disk->dir_table.entries[current_directory_index].num_children >= MAX_CHILDREN_NUM){
        perror("maximum children number reached");
        return -1;
    }

    return res;
}

//create a new file with name 'filename' in the current directory
//it will return a FileHanlde, but temporarily I put integer as return value
int createFile(Wrapper* wrapper, const char* filename){
    //before creating a file, we need to find a free entry in DirTable
    uint32_t free_entry = find_entry(wrapper);
    return free_entry;
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
