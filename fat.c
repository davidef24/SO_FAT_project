#include "fat.h"
#include <stdlib.h> //malloc
#include <stdio.h> //puts 
#include <string.h> //memset, memcpy ecc
#include <sys/mman.h> //mmap
#include <fcntl.h> //to open disk file
#include <unistd.h> // close, ftruncate
#include <stddef.h> // size_t
#include <errno.h>
#include <assert.h>

#define MMAPPED_MEMORY_SIZE sizeof(Block)*BLOCKS_NUM + sizeof(FatTable)

int fat_table_init(Wrapper* wrapper){
    FatTable* fat = &(wrapper->current_disk->fat_table);
    //skip root
    for(int i=1; i<BLOCKS_NUM; i++){
        FatEntry* entry = &(fat->entries[i]);
        if(entry == NULL) return -1;
        entry->free = 1;
        entry->next = 1; //random value, important it is different from zero initially
        
    }
    return 0;
}

Wrapper* fat_init(const char* filename){ 
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
    FatEntry* entry = &(wrapper->current_disk->fat_table.entries[0]);
    if(entry == NULL) return NULL;
    entry->entry_name[0] = '/';
    entry->entry_name[1] = '\0';
    entry->num_children=0;
    entry->directory = 1;
    entry->eof = 1; 
    entry->parent_idx = -1; // only root has -1
    
    //initialize fat table
    if(fat_table_init(wrapper) == -1) return NULL;
    return wrapper;
}

int fat_destroy(Wrapper* wrapper){ 
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

//this function checks in parent children if there is already a child named with entryName
//if finds one, returns it
//if entry_idx is not null, caller wants to know child index in directory table
//if index_in_children_array is not NULL, caller wants to know index in parent children list
FatEntry* find_by_name(Wrapper* wrapper, const char* entryName, uint32_t* first_fat_entry){
    Disk* disk = wrapper->current_disk;
    // assume current dir is first fat block of current directory
    uint32_t parent_dir_idx = wrapper->current_dir;
    FatEntry* parent_entry = &(disk->fat_table.entries[parent_dir_idx]);
    if(parent_entry == NULL) return NULL;
    //DirTable dir_table = disk->dir_table;
    Block* block= &(disk->block_list[parent_dir_idx]);
    DirectoryEntry* entry;
    FatEntry* current_entry = parent_entry;
    char* child_name;
    do {
       // printf("current_entry->next= %d\n", current_entry->next);
        //visit all directory children
        
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           entry = &(block->directory_table[i]);
           child_name = entry->name;
           if(strcmp(child_name, entryName) == 0) {
               *(first_fat_entry) = entry->first_fat_entry;
               return &(disk->fat_table.entries[entry->first_fat_entry]);
           }
        }
        current_entry = &(disk->fat_table.entries[current_entry->next]);
    } while(!current_entry->eof);
    return NULL;
}

int exceedChildLimit(Wrapper* wrapper){
    int current_directory_index = wrapper->current_dir;
    FatEntry current_entry = wrapper->current_disk->fat_table.entries[current_directory_index];
    if(current_entry.num_children >= MAX_CHILDREN_NUM){
        return 1;
    }
    return 0;
}


//find a free entry in directory table. Return value represents the index in the directory table entries array, if -1 there is an error 
int find_free_entry(Wrapper* wrapper, const char* entry_name){
    //check if current directory has reached max children number
    if (exceedChildLimit(wrapper)){
        puts("maximum children number reached");
        return -1;
    }
    Disk * disk = wrapper->current_disk;
    int res = -1;
    //we skip root directory
    for(int i=1; i< BLOCKS_NUM; i++){
        FatEntry entry = disk->fat_table.entries[i];
        if(entry.free){
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

//looks for a free entry and sets it as the eof
int32_t setFirstFatEntry(Wrapper* wrapper){
    FatTable* fat = &(wrapper->current_disk->fat_table);
    if(fat == NULL) return -1;
    FatEntry* entry;
    for(int i=0; i<BLOCKS_NUM; i++){
        entry = &(fat->entries[i]);
        if(entry == NULL) return -1;
        if (entry->free){
            entry->free = 0;
            entry->eof = 1; 
            return i;
        }
    }
    return -1;
}

//to create a file entry we need to assign name, type, first_fat_entry and add this new entry to the parent children
FileHandle* createFileEntry(Wrapper* wrapper, const char* filename, uint32_t child_entry_idx){
    //check if current directory already has a child with name entry_name
    if (find_by_name(wrapper, filename, NULL) != NULL){
        printf("Current directory already has a child named %s\n", filename);
        return NULL;
    }
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    FatEntry* parent_entry = &(disk->fat_table.entries[parent_idx]);
    FatEntry* new_entry = &(disk->fat_table.entries[child_entry_idx]);
    if(parent_entry == NULL || new_entry == NULL) return NULL;
    // init new entry
    strcpy(new_entry->entry_name, filename);
    //child_entry->type = FILE_TYPE;
    new_entry->eof = 1;
    new_entry->directory = 0;
    new_entry->free = 0;
    new_entry->parent_idx = parent_idx;
    
    //adds child to parent, assuming the limit children number has already been checked in find_entry
    Block* block= &(disk->block_list[parent_idx]);
    DirectoryEntry* entry;
    FatEntry* current_entry = parent_entry;
    do {
        //visit all directory children
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           entry = &(block->directory_table[i]);
           if(entry->name[0] == 0) {
               strcpy(entry->name, filename);
               entry->first_fat_entry = child_entry_idx;
               break;
           }
        }
        current_entry = &(disk->fat_table.entries[current_entry->next]);
    } while(!current_entry->eof);
    parent_entry->num_children++;
    // init FileHandle
    FileHandle* file_handle = (FileHandle*)malloc(sizeof(FileHandle));
    if(file_handle == NULL){
        puts("malloc error");
        return NULL;
    }
    file_handle->current_block_index = 0;
    file_handle->current_pos = 0;
    file_handle->directory_entry = child_entry_idx; //first fat entry
    file_handle->num_blocks_occupied = 1;
    file_handle->wrapper = wrapper;
    return file_handle;
}


//create a new file with name 'filename' in the current directory
FileHandle* createFile(Wrapper* wrapper, const char* filename){
    //before creating a file, we need to find a free entry in DirTable
    printf("wanna create file %s in dir with index %d in fat table\n", filename, wrapper->current_dir);
    uint32_t free_entry = find_free_entry(wrapper, filename);
    if(free_entry == -1){
        return NULL;
    }
    FileHandle* handle = createFileEntry(wrapper, filename, free_entry);
    return handle;
}

// crawl all fat entries that refer to the file, set free bit to 1 and zero block content
void freeBlocks(Wrapper* wrapper, uint32_t entry_idx){
    Disk* disk = wrapper->current_disk;
    FatEntry* current_entry = &(disk->fat_table.entries[entry_idx]);
    Block* current_block = &(disk->block_list[entry_idx]);
    if(current_block == NULL || current_entry == NULL) return;
    uint32_t next_idx;
    while(!current_entry->eof){
        //disk block
        memset(current_block, 0, sizeof(Block));
        current_entry->free = 1;
        //next block index is in 'next' field of fat entry
        next_idx = current_entry->next;
        current_entry->num_children= 0;
        //fat table 
        current_entry = &(disk->fat_table.entries[next_idx]);
        current_block = &(disk->block_list[next_idx]);
        if(current_block == NULL || current_entry == NULL) return;
    }
    memset(current_block, 0, sizeof(Block));
    current_entry->free = 1;
    //current_entry->entry_name[0] = 0;
    current_entry->num_children= 0;

}

//remove child with index in directory table equal to 'entry_idx'
int removeChild(Wrapper* wrapper, const char* to_delete_name){
    if(to_delete_name[0] == 0) return -1;
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper->current_dir;
    FatEntry* parent_entry = &(disk->fat_table.entries[parent_idx]);
    if(parent_entry == NULL) return -1;
    int8_t removed= 0;
    Block* block= &(disk->block_list[parent_idx]);
    DirectoryEntry* entry;
    FatEntry* current_entry = parent_entry;
    char* child_name;
    do {
        //visit all directory children
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           entry = &(block->directory_table[i]);
           child_name = entry->name;
           if(strcmp(child_name, to_delete_name) == 0) {
               entry->name[0] = 0;
               parent_entry->num_children--;
               removed = 1;
               break;
           }
        }
        current_entry = &(disk->fat_table.entries[current_entry->next]);
    } while(!current_entry->eof);
    if(!removed){
        return -1;
    }
    return 0;
}

//to erase a file we need to remove it from parent children, set free bit of fat entry and free all file blocks
FAT_ops_result eraseFile(FileHandle* file){
    Wrapper * wrapper = file->wrapper;
    Disk* disk = wrapper->current_disk;
    uint32_t entry_idx = file->directory_entry;
    FatEntry* entry = &(disk->fat_table.entries[entry_idx]);
    if(entry == NULL) return -1;
    
    freeBlocks(wrapper, entry_idx);
    if(removeChild(wrapper, entry->entry_name) == -1){
        return NotExistingChild;
    };
    entry->entry_name[0] = 0;
    entry->free = 1;
    free(file);
    return Success;
}

//look for a free block and update fat table values
int32_t updateFat(Disk* disk, uint32_t first){
    int32_t free = -1;
    FatEntry* entry;
    //look for a free block
    for(int i=0; i<BLOCKS_NUM; i++){
        entry = &(disk->fat_table.entries[i]);
        if(entry == NULL) return -1;
        if(entry->free){
            //only need to handle free and eof field
            entry->free = 0;
            entry->eof = 1;
            entry->directory = 0;
            free= i;
            break;
        }
    }
    if (free == -1){
        puts("There are no free blocks");
        return -1;
    }
    FatEntry* current_entry= &(disk->fat_table.entries[first]);
    if(current_entry == NULL) return -1;
    while(!current_entry->eof){
        current_entry = &(disk->fat_table.entries[current_entry->next]);
        if(current_entry == NULL) return -1;
    }
    //update previous last entry
    current_entry->eof = 0;
    current_entry->next = free;
    return free;
}

// get first free fat table entry, update previous last block and return new block
Block* getNewBlock(FileHandle* handle){
    Disk* disk = handle->wrapper->current_disk;
    int32_t new_block_idx= updateFat(disk, handle->directory_entry);
    if(new_block_idx == -1) return NULL;
    Block* new_block = &(disk->block_list[new_block_idx]);
    if(new_block == NULL) return NULL;
    return new_block;
}

// given the handle current block index, we want the index in blocks list
int32_t getBlockIndexFromHandle(FileHandle* handle){
    Disk*disk = handle->wrapper->current_disk;
    FatEntry fat_entry = disk->fat_table.entries[handle->directory_entry];
    uint32_t next_block_idx = handle->directory_entry; // start from first fat_entry
    //start from first fat_entry and get to current_block_index
    for(int i=0; i<handle->current_block_index; i++){
        //error because iteration would go on but we arrived to eof so there is a mistake in current_block_index
        if(fat_entry.eof){
            return -1;
        }
        next_block_idx = fat_entry.next;
        fat_entry = disk->fat_table.entries[next_block_idx];
    }
    return next_block_idx;
}

// write size bytes of buffer starting from current position of file handle
int fat_write(FileHandle* handle, const void* buffer, size_t size) {
    Disk* disk = handle->wrapper->current_disk;
    uint32_t current_block_remaining;
    uint32_t written = 0;
    int32_t curr_block_idx = getBlockIndexFromHandle(handle);
    if(curr_block_idx == -1){
        puts("getBlockIndexFromHandle error");
        return -1;
    }
    Block* current_block = &(disk->block_list[curr_block_idx]);
    if(current_block == NULL) return -1;
    uint32_t total_remaining = size;
    uint32_t iteration_write;
    while(written < size){
        //need new block
        if(handle->current_pos == BLOCK_SIZE){ 
            //extend file boundaries
            current_block= getNewBlock(handle);
            if(current_block == NULL) return NoFreeBlocks;
            handle->num_blocks_occupied++;
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
        memcpy(current_block->file_content + handle->current_pos, buffer + written, iteration_write);
        handle->current_pos += iteration_write;
        written += iteration_write;
        total_remaining -= iteration_write;
    }
    return written;
}

Block* getLastBlock(FileHandle* handle){
    Wrapper* wrapper = handle->wrapper;
    Disk* disk = wrapper->current_disk;
    FatEntry* current_entry = &(disk->fat_table.entries[handle->directory_entry]);
    Block* current_block = &(disk->block_list[handle->directory_entry]);
    if(current_block == NULL || current_entry == NULL) return NULL;
    uint32_t next_idx;
    while(!current_entry->eof){
        next_idx = current_entry->next;
        //fat table 
        current_entry = &(disk->fat_table.entries[next_idx]);
        current_block = &(disk->block_list[next_idx]);
        if(current_block == NULL || current_entry == NULL) return NULL;
    }
    return current_block;
}

//offset from last block beginning
int32_t findLastPosition(FileHandle* handle){
    Block* last_block = getLastBlock(handle);
    if(last_block == NULL) return -1;
    char *buffer = last_block->file_content;
    for(int i=0; i<BLOCK_SIZE; i++){
        if(buffer[i] == 0) return i-1;
    }
    //error
    return -1;
}

//start reading from handle current position
//returns number of bytes read
int fat_read(FileHandle* handle, void* buffer, size_t size) {
    Disk * disk = handle->wrapper->current_disk;
    uint32_t read_bytes = 0;
    uint32_t effective_size = size;
    uint32_t current_absolute_position = (handle->current_block_index)*BLOCK_SIZE + handle->current_pos;
    uint32_t last_pos = (handle->num_blocks_occupied-1) * BLOCK_SIZE + findLastPosition(handle);
    // check if size parameter exceeds file dimension
    if(size > last_pos - current_absolute_position) {
        // read till the last position of file
        effective_size = (last_pos - current_absolute_position)+1;
    }
    int32_t curr_block_idx = getBlockIndexFromHandle(handle);
    if(curr_block_idx == -1){
        puts("getBlockIndexFromHandle error");
        return -1;
    }
    Block* current_block = &(disk->block_list[curr_block_idx]);
    if(current_block == NULL) return -1;
    uint32_t iteration_read, current_block_remaining;
    while(read_bytes < effective_size){
        if(handle->current_pos == BLOCK_SIZE){
            //number of bytes to read exceed file blocks number, so we finish here
            if((handle->current_block_index+1) >= handle->num_blocks_occupied) return iteration_read;
            handle->current_block_index++;
            handle->current_pos = 0;
            curr_block_idx = getBlockIndexFromHandle(handle);
            //there are no more blocks
            if(curr_block_idx == -1){
                return read_bytes;
            }
            current_block = &(disk->block_list[curr_block_idx]);
            if(current_block == NULL) return -1;
        }
        current_block_remaining = BLOCK_SIZE - handle->current_pos;
        //we are in the last block
        if(effective_size-read_bytes < current_block_remaining){
            iteration_read = effective_size-read_bytes;
        }
        //we will need to read all reamining bytes of current block and then get next block to finish read
        else{
            iteration_read = current_block_remaining;
        }
        memcpy(buffer + read_bytes, current_block->file_content + handle->current_pos, iteration_read);
        handle->current_pos += iteration_read;
        read_bytes += iteration_read;
    }
    return read_bytes;
}

void updateHandle(FileHandle* handle, uint32_t new_position){
    uint32_t new_block_idx = new_position / BLOCK_SIZE;
    uint32_t new_current_pos = new_position % BLOCK_SIZE;
    handle->current_pos = new_current_pos;
    handle->current_block_index = new_block_idx;
}

//returns offset from file beginning
int fat_seek(FileHandle* handle, int32_t offset, FatWhence whence){
    assert(whence == FAT_CUR || whence == FAT_END || whence == FAT_SET);
    uint32_t absolute_position; // depends on whence
    uint32_t new_position;
    int32_t last_pos;
    uint32_t max_position = (handle->num_blocks_occupied)*BLOCK_SIZE;
    if(whence == FAT_END){
        if(offset > 0) {
            return InvalidSeekOffset;
        }
        else{
            last_pos = findLastPosition(handle);
            if(last_pos == -1) return InvalidSeekOffset;
            absolute_position = (handle->num_blocks_occupied-1)*BLOCK_SIZE + (last_pos +1); //+1 to count position 0
            new_position = absolute_position + offset;
            if(new_position < 0) return InvalidSeekOffset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
    else if(whence == FAT_SET){
        if(offset < 0){
            return InvalidSeekOffset;
        } 
        else{
            //in this case offset is referred to the beginning of the file
            absolute_position = 0;
            new_position = absolute_position + offset;
            if(new_position > max_position) return InvalidSeekOffset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
    else{  // FAT_CUR
        //check if the offset is too large
        absolute_position = (handle->current_block_index) * BLOCK_SIZE + handle->current_pos;
        if(absolute_position + offset < 0 || absolute_position + offset > max_position){
            return InvalidSeekOffset;
        }
        else{
            new_position = absolute_position + offset;
            updateHandle(handle, new_position);
            return new_position;
        }
    }
}

FatEntry* createDirEntry(Wrapper *wrapper, const char* dirName, uint32_t new_entry_idx){
    if (find_by_name(wrapper, dirName, NULL) != NULL){
        printf("Current directory already has a child named %s\n", dirName);
        return NULL;
    }
    Disk * disk = wrapper->current_disk;
    uint32_t parent_idx = wrapper ->current_dir;
    FatEntry* parent_entry = &(disk->fat_table.entries[parent_idx]);
    FatEntry* new_entry = &(disk->fat_table.entries[new_entry_idx]);
    if(parent_entry == NULL || new_entry == NULL) return NULL;
    strcpy(new_entry->entry_name, dirName);
    new_entry->eof = 1;
    new_entry->free= 0;
    new_entry->directory = 1;
    new_entry->num_children = 0;
    new_entry->parent_idx = parent_idx;
    //adds child to parent, assuming the limit children number has already been checked in find_entry
    Block* block= &(disk->block_list[parent_idx]);
    DirectoryEntry* entry;
    FatEntry* current_entry = parent_entry;    
    do {
        
        //visit all directory children
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           entry = &(block->directory_table[i]);
           if(entry->name[0] == 0) {
               strcpy(entry->name, dirName);
               entry->first_fat_entry = new_entry_idx;
               break;
           }
          
        }
        current_entry = &(disk->fat_table.entries[current_entry->next]);
    } while(!current_entry->eof);
    parent_entry->num_children++;
    
    return new_entry;
}

FAT_ops_result createDir(Wrapper* wrapper, const char* dirName){
    printf("wanna create dir %s in directory with index %d in fat table\n", dirName, wrapper->current_dir);
    int32_t new_entry_idx = find_free_entry(wrapper, dirName);
    if (new_entry_idx == -1) {
        return NoFreeEntries;
    }
    FatEntry* new_entry = createDirEntry(wrapper, dirName, new_entry_idx);
    if(new_entry == NULL){
        return AlreadyExistingChild;
    }
    return Success;
}
FAT_ops_result eraseDirRecursive(Wrapper* wrapper, const char* dirName, int32_t parent_idx, uint32_t to_delete_idx) { //, uint32_t index_in_children_list){
    Disk* disk = wrapper->current_disk;
    FatEntry* parent_entry = &(disk->fat_table.entries[parent_idx]);
    if(parent_entry == NULL) return NoSuchEntryInDirTable;
    FatEntry* to_delete = &(disk->fat_table.entries[to_delete_idx]);
    if(to_delete == NULL) return NoSuchEntryInDirTable;
    FatEntry* child;
    // eraseDir operate from parent of directory to delete, so to remove 'to_delete' children we need to change current directory
    wrapper->current_dir = to_delete_idx;
    Block* block= &(disk->block_list[to_delete_idx]);
    DirectoryEntry* dir_entry; //represent child entry in directory file
    FatEntry current_entry = *(to_delete);
    char* child_name;
    do {
        //visit all directory children
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           dir_entry = &(block->directory_table[i]);
           if(dir_entry->name[0] != 0){
               child = &(disk->fat_table.entries[dir_entry->first_fat_entry]);
               child_name = dir_entry->name;
               if(child->directory){
                   if(eraseDirRecursive(wrapper, child_name, to_delete_idx, dir_entry->first_fat_entry) < 0) return -1;
               }
               else{
                   freeBlocks(wrapper, dir_entry->first_fat_entry);
                   
               }
               // free fat entry and directory entry in dircetory file
               child->entry_name[0] = 0;
               dir_entry->name[0] = 0;
               child->free = 1;
               
               
           }
        }
        current_entry = disk->fat_table.entries[current_entry.next];
    } while(!current_entry.eof);
    // restore current directory
    wrapper->current_dir = parent_idx;
    printf("calling from %s directory to remove %s child\n", parent_entry->entry_name, dirName);
    removeChild(wrapper, dirName);
    to_delete->free = 1;
    to_delete->entry_name[0] = 0;
    to_delete->num_children = 0;
    return Success;
}

FAT_ops_result eraseDir(Wrapper* wrapper, const char* dirName){
    uint32_t parent_dir_idx = wrapper->current_dir;
    uint32_t to_delete_idx;
    // to_delete_idx to know index in directory table
    FatEntry* to_delete = find_by_name(wrapper, dirName, &to_delete_idx); //DIRECTORY_TYPE, &to_delete_idx, &index_in_children_list);
    if(to_delete == NULL){
        puts("No such directory to delete");
        return NoSuchDirectory;
    }
    printf("Gonna delete directory with this content: \n");
    changeDir(wrapper, dirName);
    listDir(wrapper);
    changeDir(wrapper, "..");
    return eraseDirRecursive(wrapper, dirName, parent_dir_idx, to_delete_idx); //index_in_children_list);
}

FAT_ops_result goBack(Wrapper* wrapper){
    FatEntry current_dir_entry = wrapper->current_disk->fat_table.entries[wrapper->current_dir];
    if(strcmp(current_dir_entry.entry_name, "/") == 0){
        puts("current directory is root, can't go back");
        return BackFromRootError;
    }
    else{
        uint32_t new_dir_idx = current_dir_entry.parent_idx;
        wrapper->current_dir = new_dir_idx;
        return Success;
    }
}

FAT_ops_result changeDir(Wrapper* wrapper, const char* newDirectory){
    uint32_t new_dir_idx;
    if(strcmp(newDirectory, "..") == 0) return goBack(wrapper);
    FatEntry* new_dir = find_by_name(wrapper, newDirectory, &new_dir_idx);
    if(new_dir == NULL){
        return NoSuchDirectory;
    }
    wrapper->current_dir = new_dir_idx;
    return Success;
} 

void listDir(Wrapper* wrapper){
    uint32_t current_dir_idx = wrapper->current_dir;
    Disk* disk = wrapper->current_disk;
    FatEntry current_dir_entry = disk->fat_table.entries[current_dir_idx];
    printf("***********listing directory %s******************\n", current_dir_entry.entry_name);
    Block* block= &(disk->block_list[current_dir_idx]);
    DirectoryEntry* entry;
    FatEntry current_entry = current_dir_entry;
    char* child_name;
    uint32_t parent_num_children= current_dir_entry.num_children;
    int touched= 0;
    do {
        //visit all directory children
        for(int i = 0; i< MAX_CHILDREN_NUM; i++){
           entry = &(block->directory_table[i]);
           child_name = entry->name;
           if(child_name[0] != 0) {
               printf("-----%s\n", child_name); 
               touched++;
               if(touched >= parent_num_children) {
                   puts("***********************");
                   return;
               }
           }
        }
        current_entry = disk->fat_table.entries[current_entry.next];
    } while(!current_entry.eof);
    
}
