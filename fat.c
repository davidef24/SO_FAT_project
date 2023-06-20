#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //perror 
#include <string.h> //memset, memcpy ecc
#include <sys/mman.h> //mmap
#include <fcntl.h> //to open disk file
#include <unistd.h> // close, ftruncate
#include <stddef.h> // size_t
#include <errno.h>



void Fat_init(Wrapper* wrapper){
    FatTable* fat = &(wrapper->current_disk->fat_table);
    for(int i=0; i<BLOCKS_NUM; i++){
        FatEntry* entry = &(fat->entries[i]);
        entry->state = FREE_ENTRY;
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
int find_free_entry(Wrapper* wrapper, const char* entry_name){
    //check if current directory has reached max children number
    if (exceedChildLimit(wrapper)){
        perror("maximum children number reached");
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
int32_t setFirstFatEntry(Wrapper* wrapper){
    FatTable* fat = &(wrapper->current_disk->fat_table);
    for(int i=0; i<BLOCKS_NUM; i++){
        FatEntry* entry = &(fat->entries[i]);
        if (entry->state == FREE_ENTRY){
            entry->state = BUSY_ENTRY;
            entry->value = LAST_ENTRY; 
            return i;
        }
    }
    return -1;
}

//assigns name, type, first_fat_entry and adds child to parent
FileHandle* createFileEntry(Wrapper* wrapper, const char* filename, uint32_t child_entry_idx){
    //check if current directory already has a child with name entry_name
    if (alreadyHasChildren(wrapper, filename) == -1){
        printf("Current directory already has a child named %s\n", filename);
        return NULL;
    }
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_idx]);
    DirEntry* child_entry = &(disk->dir_table.entries[child_entry_idx]);
    uint32_t name_size = strlen(filename) +1;
    memcpy(child_entry->entry_name, filename, name_size);
    child_entry->type = FILE_TYPE;
    //assigns to the new file a first entry in the fat table, which will be set to  busy state and LAST_ENTRY value
    if((child_entry->first_fat_entry = setFirstFatEntry(wrapper) == -1)){
        perror("there are no free entries in fat table");
        return NULL;
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
    FileHandle* file_handle = (FileHandle*)malloc(sizeof(FileHandle));
    if(file_handle == NULL){
        perror("malloc error");
        return NULL;
    }
    file_handle->current_block_index = 0;
    file_handle->current_pos = 0;
    file_handle->directory_entry = child_entry_idx;
    file_handle->wrapper = wrapper;
    return file_handle;
}


//create a new file with name 'filename' in the current directory
//it will return a FileHanlde, but temporarily I put integer as return value
FileHandle* createFile(Wrapper* wrapper, const char* filename){
    //before creating a file, we need to find a free entry in DirTable
    uint32_t free_entry = find_free_entry(wrapper, filename);
    if(free_entry == -1){
        return NULL;
    }
    FileHandle* handle = createFileEntry(wrapper, filename, free_entry);
    return handle;
}

// all fat entries referring file to delete will be set to FREE_ENTRY
void freeBlocks(Wrapper* wrapper, DirEntry* entry){
    Disk* disk = wrapper->current_disk;
    uint32_t current_entry_idx = entry->first_fat_entry;
    FatEntry current_entry = disk->fat_table.entries[current_entry_idx];
    Block* current_block = &(disk->block_list[current_entry_idx]);
    
    while(current_entry.value != LAST_ENTRY){
        //disk block
        memset(current_block, 0, sizeof(Block));
        current_entry.state = FREE_ENTRY;
        //next block index is the entry value of the fat table
        uint32_t next_idx = current_entry.value;
        //fat table 
        current_entry = disk->fat_table.entries[next_idx];
        
        current_block = &(disk->block_list[next_idx]);
    }
    memset(current_block, 0, sizeof(Block));

}

//for now, in this project the remove and create functions operate from current directory
void removeChild(Wrapper* wrapper, uint32_t entry_idx){
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_idx]);
    //we set children id sequencially, so it's useless iteratore for MAX_CHILD_NUM times
    for(int i=0; i<parent_entry->num_children; i++){
        uint32_t child_idx = parent_entry->children[i];
        if (child_idx == entry_idx){
            parent_entry->children[i] = 0;
            break;
        }
    }
    parent_entry->num_children--;
}

//to erase a file we need to remove it from parent children, set fat table entries to free and free all file blocks
int eraseFile(FileHandle* file){
    Wrapper * wrapper = file->wrapper;
    Disk* disk = wrapper->current_disk;
    uint32_t entry_idx = file->directory_entry;
    DirEntry* entry = &(disk->dir_table.entries[entry_idx]);
    freeBlocks(wrapper, entry);
    removeChild(wrapper, entry_idx);
    free(file);
    return 0;
}

//entry which is now last, will have a new value indicating the new entry 
uint32_t updateFat(Disk* disk, uint32_t first){
    uint32_t free = -1;
    for(int i=0; i<BLOCK_SIZE; i++){
        FatEntry* entry = &(disk->fat_table.entries[i]);
        if(entry->state == FREE_ENTRY){
            entry->state = BUSY_ENTRY;
            entry->value = LAST_ENTRY;
            free= i;
            break;
        }
    }
    if (free == -1){
        perror("no free space");
        return -1;
    }
    FatEntry* current_entry= &(disk->fat_table.entries[first]);
    while(current_entry->value != LAST_ENTRY){
        current_entry = &(disk->fat_table.entries[current_entry->value]);
    }
    current_entry->value = free;
    return free;

}

Block* getNewBlock(FileHandle* handle){
    Disk* disk = handle->wrapper->current_disk;
    DirEntry* file_entry = &(disk->dir_table.entries[handle->directory_entry]);
    uint32_t new_block_idx= updateFat(disk, file_entry->first_fat_entry);
    Block* new_block = &(disk->block_list[new_block_idx]);
    return new_block;
}

//handle contains block index. From this value we need the the index in block list, which is different
uint32_t getBlockFromHandleIndex(FileHandle* handle){
    Disk*disk = handle->wrapper->current_disk;
    DirEntry dir_entry = disk->dir_table.entries[handle->directory_entry];
    FatEntry fat_entry = disk->fat_table.entries[dir_entry.first_fat_entry];
    uint32_t next_block_idx = dir_entry.first_fat_entry;
    for(int i=0; i<handle->current_block_index; i++){
        next_block_idx = fat_entry.value;
        fat_entry = disk->fat_table.entries[next_block_idx];
    }
    return next_block_idx;
}

int fat_write(FileHandle* handle, const void* buffer, size_t size) {
    Disk* disk = handle->wrapper->current_disk;
    uint32_t curr_pos=  handle->current_pos;
    uint32_t current_block_remaining;
    uint32_t written = 0;
    uint32_t curr_block_idx = getBlockFromHandleIndex(handle);
    uint32_t new_blocks_num=0;
    Block* current_block = &(disk->block_list[curr_block_idx]);
    uint32_t total_remaining = size;
    uint32_t iteration_write;
    //no need to iterate 
    if(size < BLOCK_SIZE - curr_pos){
          memcpy(current_block+curr_pos, buffer, size);
          handle->current_pos += size;
          written = size;
          return written;
    }
    int it=0;
    while(written < size){
        if(handle->current_pos == BLOCK_SIZE){
            current_block= getNewBlock(handle);
            new_blocks_num++;
            handle->current_block_index++;
            handle->current_pos = 0;
        }
        current_block_remaining = BLOCK_SIZE - handle->current_pos;
        if(total_remaining < current_block_remaining){
            iteration_write = total_remaining;
        }
        else{
            iteration_write = current_block_remaining;
        }
        memcpy(current_block + curr_pos, buffer + written, iteration_write);
        handle->current_pos += iteration_write;
        written += iteration_write;
        total_remaining -= iteration_write;
        it ++;
    }
    
    return written;
}

//start reading from current position of the file handle size bytes
int fat_read(FileHandle* handle, void* buffer, size_t size) {
    return 0;
}

//returns offset from file beginning
int fat_seek(FileHandle* handle, int32_t offset, FatWhence whence){
    printf("-------------------fat_seek-------------");
    printf("initial file_handle state: current_pos: %d\tcurrent_block_index: %d\n", handle->current_pos, handle->current_block_index);
    uint32_t block_offset, blocks_num;
    if(whence == FAT_END){
        if(offset > 0) {
            perror("invalid offset");
            return -1;
        }
        else{
            blocks_num = offset / BLOCK_SIZE;
            block_offset = handle->current_pos + offset % BLOCK_SIZE;
            printf("blocks_num = %d\t block_offset= %d\n", blocks_num, block_offset);
            return 0;
        }
    }
    else if(whence == FAT_SET){
        if((int) offset < 0){
            perror("invalid offset");
            return -1;
        } 
        else{
            blocks_num = offset / BLOCK_SIZE;
            block_offset = handle->current_pos + offset % BLOCK_SIZE;
            printf("blocks_num = %d\t block_offset= %d\n", blocks_num, block_offset);
            return 0;
        }
    }
    else{
        //check if the offset is too large
        uint32_t total_file_size = (handle->current_block_index+1)*BLOCK_SIZE + handle->current_pos;
        uint32_t blocks_occupied = handle->current_block_index +1;
        if(total_file_size + offset < 0 || total_file_size + offset > blocks_occupied * BLOCK_SIZE){
            perror("invalid offset");
            return -1;
        }
        else{
            blocks_num = offset / BLOCK_SIZE;
            block_offset = offset % BLOCK_SIZE;
            printf("blocks_num = %d\t block_offset= %d\n", blocks_num, block_offset);
            return 0;
        }
    }
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
        printf("-----%s\n", child.entry_name);
    }
}
