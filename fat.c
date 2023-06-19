#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //perror 
#include <string.h> //memset, memcpy ecc
#include <sys/mman.h> //mmap
#include <fcntl.h> //to open disk file
#include <unistd.h> // close, ftruncate
#include <stddef.h> // size_t



void Fat_init(Wrapper* wrapper){
    FatTable fat = wrapper->current_disk->fat_table;
    for(int i=0; i<BLOCKS_NUM; i++){
        FatEntry entry = fat.entries[i];
        entry.state = FREE_ENTRY;
    }
}

Wrapper* Disk_init(const char* filename){
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
    entry->num_children=0;
    memset(entry->children, 0x00, sizeof(entry->children));
    
    //initialize fat table
    //for now, if an entry value is 0 this means that it is free
    Fat_init(wrapper);
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


int alreadyHasChildren(Wrapper* wrapper, const char* entry_name){
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    DirEntry parent_entry = disk->dir_table.entries[parent_idx];
    uint32_t parent_children_num = parent_entry.num_children;
    for (int i = 0; i < parent_children_num; i++){
        uint32_t child_entry_idx = parent_entry.children[i];
        DirEntry child_entry = disk->dir_table.entries[child_entry_idx];
        const char* child_name = child_entry.entry_name;
        if (strcmp(entry_name, child_name) == 0){
            return -1;
        }
    }
    return 0;
}

int exceedChildLimit(Wrapper* wrapper){
    int current_directory_index = wrapper->current_dir;
    DirEntry current_entry = wrapper->current_disk->dir_table.entries[current_directory_index];
    if(current_entry.num_children >= MAX_CHILDREN_NUM){
        return 1;
    }
    return 0;
}


//result represents the index in the directory table entries array, if -1 there is an error 
int find_entry(Wrapper* wrapper, const char* entry_name){
    //check if current directory has reached max children number
    if (exceedChildLimit(wrapper)){
        perror("maximum children number reached");
        return -1;
    }
    //check if current directory already has a child with name entry_name
    if (alreadyHasChildren(wrapper, entry_name) == -1){
        printf("Current directory already has a child named %s\n", entry_name);
        return -1;
    }

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
    return res;
}

#define LAST_ENTRY ((uint32_t) (~0))

//first, it looks for a free entry and sets it as the last since initially the allocation is of one block
int32_t firstFatEntry(Wrapper* wrapper){
    FatTable fat = wrapper->current_disk->fat_table;
    for(int i=0; i<BLOCKS_NUM; i++){
        FatEntry entry = fat.entries[i];
        if (entry.state == FREE_ENTRY){
            entry.state = BUSY_ENTRY;
            entry.value = LAST_ENTRY; 
            return i;
        }
    }
    return -1;
}

//assigns name, type, first_fat_entry and adds child to parent
int createFileEntry(Wrapper* wrapper, const char* filename, uint32_t child_entry_idx){
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_idx]);
    DirEntry* child_entry = &(disk->dir_table.entries[child_entry_idx]);
    uint32_t name_size = strlen(filename);
    memcpy(child_entry->entry_name, filename, name_size);
    child_entry->type = FILE_TYPE;
    if((child_entry->first_fat_entry = firstFatEntry(wrapper) == -1)){
        perror("there are no free entries in fat table");
        return -1;
    }
    //adds child to parent, assuming the limit children number has already been checked in find_entry
    for(int i=0; i<MAX_CHILDREN_NUM;i++){
        //assume 0 as value of children[i] means it's free (any directory can have root as child)
        if (parent_entry->children[i] == 0){
            puts("Found free entry");
            parent_entry->children[i] = child_entry_idx;
            break;
        }
    } 
    parent_entry->num_children++;
    return 0;
}


//create a new file with name 'filename' in the current directory
//it will return a FileHanlde, but temporarily I put integer as return value
int createFile(Wrapper* wrapper, const char* filename){
    //before creating a file, we need to find a free entry in DirTable
    uint32_t free_entry = find_entry(wrapper, filename);
    if(free_entry == -1){
        return -1;
    }
    createFileEntry(wrapper, filename, free_entry);
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
void listDir(Wrapper* wrapper){
    uint32_t current_dir_idx = wrapper->current_dir;
    Disk* disk = wrapper->current_disk;
    DirEntry current_dir_entry = disk->dir_table.entries[current_dir_idx];
    printf("Listing content of directory %s\n", current_dir_entry.entry_name);
    for(int i=0; i < current_dir_entry.num_children; i++){
        uint32_t child_idx = current_dir_entry.children[i];
        DirEntry child = disk->dir_table.entries[child_idx];
        printf("%s\n", child.entry_name);
    }
}
