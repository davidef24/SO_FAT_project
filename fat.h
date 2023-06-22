#pragma once
#include <stdint.h> //uint
#include <stddef.h> // size_t

#define BLOCKS_NUM 2048
#define BLOCK_SIZE 32
#define MAX_NAME_LENGTH 64
#define DIRECTORY_ENTRIES_NUM 64
#define MAX_CHILDREN_NUM 16

#define MMAPPED_MEMORY_SIZE sizeof(Block)*BLOCKS_NUM + sizeof(FatTable) + sizeof(DirTable)

typedef enum DirEntryType{
	FILE_TYPE,
	DIRECTORY_TYPE
} DirEntryType;

typedef struct DirEntry {
    char entry_name[MAX_NAME_LENGTH];
    DirEntryType type;
    uint32_t first_fat_entry;
    uint32_t parent_idx;
    uint32_t num_children;
    uint8_t children[MAX_CHILDREN_NUM];
} DirEntry;

typedef enum FatWhence{
    FAT_CURRENT=0,
    FAT_END =1,
    FAT_SET= 2
} FatWhence;

typedef enum FatEntryState {
    FREE_ENTRY,
    BUSY_ENTRY
}FatEntryState;

typedef struct FatEntry {
    FatEntryState state;
    uint32_t value;
}FatEntry;

typedef struct FatTable {
    FatEntry entries[BLOCKS_NUM];
} FatTable;

typedef struct DirTable {
    DirEntry entries[DIRECTORY_ENTRIES_NUM];
} DirTable;

typedef struct Block{
    char block_content[BLOCK_SIZE];
} Block;


typedef struct Disk{
    FatTable fat_table;
    DirTable dir_table;
    Block block_list[BLOCKS_NUM];
} Disk;

typedef struct Wrapper{
    uint32_t disk_fd;
    uint32_t current_dir;
    Disk * current_disk;
} Wrapper;

typedef struct FileHandle {
	uint32_t current_pos;
	uint32_t current_block_index;
	uint32_t directory_entry;
    Wrapper*  wrapper;
    uint32_t last_pos_occupied;    
} FileHandle;

//initialize a new disk which will be an mmapped file and returns a wrapper
Wrapper* Disk_init(const char* filename);

//initialize fat entries to free state
void Fat_init(Wrapper* wrapper);

int Fat_destroy(Wrapper* wrapper);

FileHandle* createFile(Wrapper* wrapper, const char* filename);

int eraseFile(FileHandle* file);
int fat_write(FileHandle* handle, const void* buffer, size_t size) ;
int fat_read(FileHandle* handle, void* buffer, size_t size);
int fat_seek(FileHandle* handle, int32_t offset, FatWhence whence);
int createDir(Wrapper* wrapper, const char* dirName);
int eraseDir(Wrapper* wrapper,const char* dirName);
int changeDir(Wrapper* wrapper,const char* newDirectory);

//lists all children of current dirctory
void listDir(Wrapper* wrapper);