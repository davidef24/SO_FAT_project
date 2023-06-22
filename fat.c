#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //puts 
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
        puts("open error");
        return NULL;
    }
    if(ftruncate(wrapper->disk_fd, sizeof(Disk)) == -1){
        free(wrapper);
        puts("ftruncate error");
        return NULL;
    }		
    wrapper->current_disk = (Disk*) mmap(NULL, sizeof(Disk), PROT_READ | PROT_WRITE, MAP_SHARED, wrapper->disk_fd, 0);
    if (wrapper->current_disk == MAP_FAILED){
        puts("mmap error");
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
        puts("unmap error");
        return -1;
    }
    if(close(wrapper->disk_fd) < 0){
        puts("close error");
        return -1;
    }
    free(wrapper);
    return 0;
}

DirEntry* find_by_name(Wrapper* wrapper, const char* entryName, DirEntryType type, uint32_t* entry_idx){
    Disk* disk = wrapper->current_disk;
    uint32_t parent_dir_idx = wrapper->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_dir_idx]);
    uint32_t parent_children_num = parent_entry->num_children;
    for (int i = 0; i < parent_children_num; i++){
        uint32_t child_entry_idx = parent_entry->children[i];
        DirEntry* child_entry = &(disk->dir_table.entries[child_entry_idx]);
        const char* child_name = child_entry->entry_name;
        if (strcmp(entryName, child_name) == 0 && child_entry->type == type){
            if(entry_idx != NULL){
                *entry_idx = child_entry_idx;
            }
            return child_entry;
        }
    }
    return NULL;
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
        puts("maximum children number reached");
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
        puts("no free entries");
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
    if (find_by_name(wrapper, filename, FILE_TYPE, NULL) != NULL){
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
        puts("there are no free entries in fat table");
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
        puts("malloc error");
        return NULL;
    }
    file_handle->current_block_index = 0;
    file_handle->current_pos = 0;
    file_handle->directory_entry = child_entry_idx;
    file_handle->last_pos_occupied = 0;
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
int removeChild(Wrapper* wrapper, uint32_t entry_idx){
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_idx]);
    //we set children id sequencially, so it's useless iteratore for MAX_CHILD_NUM times
    int8_t removed= 0;
    for(int i=0; i<parent_entry->num_children; i++){
        uint32_t child_idx = parent_entry->children[i];
        if (child_idx == entry_idx){
            parent_entry->children[i] = 0;
            removed = 1;
            break;
        }
    }
    if(!removed){
        puts("There is no child with that entry_idx");
        return -1;
    }
    parent_entry->num_children--;
    return 0;
}

//to erase a file we need to remove it from parent children, set fat table entries to free and free all file blocks
int eraseFile(FileHandle* file){
    Wrapper * wrapper = file->wrapper;
    Disk* disk = wrapper->current_disk;
    uint32_t entry_idx = file->directory_entry;
    DirEntry* entry = &(disk->dir_table.entries[entry_idx]);
    freeBlocks(wrapper, entry);
    if(removeChild(wrapper, entry_idx) == -1){
        return -1;
    };
    entry->entry_name[0] = 0;
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
        puts("no free space");
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
uint32_t getBlockFromIndex(FileHandle* handle){
    Disk*disk = handle->wrapper->current_disk;
    DirEntry dir_entry = disk->dir_table.entries[handle->directory_entry];
    FatEntry fat_entry = disk->fat_table.entries[dir_entry.first_fat_entry];
    uint32_t next_block_idx = dir_entry.first_fat_entry;
    for(int i=0; i<handle->current_block_index; i++){
        next_block_idx = fat_entry.value;
        if(next_block_idx == LAST_ENTRY){
            return -1;
        }
        fat_entry = disk->fat_table.entries[next_block_idx];
    }
    return next_block_idx;
}

int fat_write(FileHandle* handle, const void* buffer, size_t size) {
    Disk* disk = handle->wrapper->current_disk;
    uint32_t curr_pos=  handle->current_pos;
    uint32_t current_block_remaining;
    uint32_t written = 0;
    uint32_t curr_block_idx = getBlockFromIndex(handle);
    if(curr_block_idx == -1){
        puts("getBlockFromIndex error");
        return -1;
    }
    uint32_t new_blocks_num=0;
    Block* current_block = &(disk->block_list[curr_block_idx]);
    uint32_t total_remaining = size;
    uint32_t iteration_write;
    //no need to iterate 
    if(size < BLOCK_SIZE - curr_pos){
          memcpy(current_block->block_content +curr_pos, buffer, size);
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
        memcpy(current_block->block_content + curr_pos, buffer + written, iteration_write);
        handle->current_pos += iteration_write;
        written += iteration_write;
        handle->last_pos_occupied += iteration_write;
        total_remaining -= iteration_write;
        it ++;
    }
    
    return written;
}

//start reading from handle current position
//returns number of bytes read
int fat_read(FileHandle* handle, void* buffer, size_t size) {
    Disk * disk = handle->wrapper->current_disk;
    uint32_t read_bytes = 0;
    uint32_t total_remaining = size;
    uint32_t curr_block_idx = getBlockFromIndex(handle);
    if(curr_block_idx == -1){
        puts("getBlockFromIndex error");
        return -1;
    }
    Block* current_block = &(disk->block_list[curr_block_idx]);
    
    uint32_t curr_pos=  handle->current_pos;
    printf("curr_pos: %d\n", curr_pos);
    printf("Current block content: %s\n", current_block->block_content);
    uint32_t iteration_read, current_block_remaining;
    
    //read is performed only in a disk block 
    if(size < BLOCK_SIZE - curr_pos){
          memcpy(buffer, current_block->block_content +curr_pos, size);
          handle->current_pos += size;
          iteration_read = size;
          return iteration_read; // (???) what if reading from FAT_END n bytes forward till the end of the block?
    }
    while(read_bytes < size){
        if(handle->current_pos == BLOCK_SIZE){
            handle->current_block_index++;
            handle->current_pos = 0;
            curr_block_idx = getBlockFromIndex(handle);
            if(curr_block_idx == -1){
                puts("buffer overflow");
                return -1;
            }
            current_block = &(disk->block_list[curr_block_idx]);
        }
        current_block_remaining = BLOCK_SIZE - handle->current_pos;
        if(total_remaining < current_block_remaining){
            iteration_read = total_remaining;
        }
        else{
            iteration_read = current_block_remaining;
        }
        memcpy(buffer + read_bytes, current_block->block_content + curr_pos, iteration_read);
        handle->current_pos += iteration_read;
        read_bytes += iteration_read;
        total_remaining -= iteration_read;
    }
    return iteration_read;
}

void updateHandle(FileHandle* handle, uint32_t new_position){
    uint32_t new_block_idx = new_position / BLOCK_SIZE;
    uint32_t new_current_pos = new_position % BLOCK_SIZE;
    handle->current_pos = new_current_pos;
    handle->current_block_index = new_block_idx;
}

//returns offset from file beginning
int fat_seek(FileHandle* handle, int32_t offset, FatWhence whence){
    uint32_t absolute_position = (handle->current_block_index) * BLOCK_SIZE + handle->current_pos;
    uint32_t new_position;
    if(whence == FAT_END){
        if(offset > 0) {
            puts("invalid seek offset");
            return -1;
        }
        else{
            absolute_position = handle->last_pos_occupied;
            new_position = absolute_position + offset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
    else if(whence == FAT_SET){
        if((int) offset < 0){
            puts("invalid seek offset");
            return -1;
        } 
        else{
            //in this case offset is referred to the beginning of the file
            absolute_position = 0;
            new_position = absolute_position + offset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
    else{
        //check if the offset is too large
        uint32_t available_size = (handle->current_block_index+1)*BLOCK_SIZE;
        if(absolute_position + offset < 0 || absolute_position + offset > available_size){
            puts("invalid seek offset");
            return -1;
        }
        else{
            absolute_position = (handle->current_block_index) * BLOCK_SIZE + handle->current_pos;
            new_position = absolute_position + offset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
}

DirEntry* createDirEntry(Wrapper *wrapper, const char* dirName, uint32_t new_entry_idx){
    if (find_by_name(wrapper, dirName, DIRECTORY_TYPE, NULL) != NULL){
        printf("Current directory already has a child named %s\n", dirName);
        return NULL;
    }
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_idx]);
    DirEntry* new_entry = &(disk->dir_table.entries[new_entry_idx]);
    uint32_t name_size = strlen(dirName) +1;
    memcpy(new_entry->entry_name, dirName, name_size);
    new_entry->type = DIRECTORY_TYPE;
    //adds child to parent, assuming the limit children number has already been checked in find_entry
    for(int i=0; i<MAX_CHILDREN_NUM;i++){
        //assume 0 as value of children[i] means it's free (any directory can have root as child)
        if (parent_entry->children[i] == 0){
            puts("Found free entry");
            parent_entry->children[i] = new_entry_idx;
            break;
        }
    } 
    parent_entry->num_children++;
    new_entry->num_children = 0;
    memset(new_entry->children, 0x00, sizeof(new_entry->children));
    return new_entry;
}

int createDir(Wrapper* wrapper, const char* dirName){
    int32_t new_entry_idx = find_free_entry(wrapper, dirName);
    if (new_entry_idx == -1){
        puts("no free entry");
        return -1;
    }
    DirEntry* new_entry = createDirEntry(wrapper, dirName, new_entry_idx);
    if(new_entry == NULL){
        return -1;
    }
    return 0;
}



int eraseDir(Wrapper* wrapper, const char* dirName){
    Disk* disk = wrapper->current_disk;
    uint32_t parent_dir_idx = wrapper->current_dir;
    DirEntry* to_delete = find_by_name(wrapper, dirName, DIRECTORY_TYPE, NULL);
    if(to_delete == NULL){
        puts("No directory with given name");
        return -1;
    }
    for(int i=0; i < to_delete->num_children; i++){
        uint32_t child_idx = to_delete->children[i];
        DirEntry* child = &(disk->dir_table.entries[child_idx]);
        if(removeChild(wrapper, child_idx) == -1){
                return -1;
        };
        child->entry_name[0] = 0;
        child->num_children = 0;
        if(child->type == FILE_TYPE){
            freeBlocks(wrapper, child);
        }
        else if(child->type == DIRECTORY_TYPE){
            wrapper->current_dir = child_idx;
            if(eraseDir(wrapper, child->entry_name) == -1){
                return -1;
            }
            wrapper->current_dir = parent_dir_idx;
        }
    }
    return 0;
}

int changeDir(Wrapper* wrapper, const char* newDirectory){
    uint32_t new_dir_idx;
    DirEntry* new_dir = find_by_name(wrapper, newDirectory, DIRECTORY_TYPE, &(new_dir_idx));
    if(new_dir == NULL){
        puts("No such directory");
        return -1;
    }
    wrapper->current_dir = new_dir_idx;
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
