#pragma once
#include <stdint.h> //uint
#include <stddef.h> // size_t

#define BLOCKS_NUM 512
#define BLOCK_SIZE 32
#define DIRECTORY_ENTRIES_NUM 64
#define MAX_CHILDREN_NUM 16
#define MAX_NAME_LENGTH 64



#define FREE_DIR_CHILD_ENTRY -1

typedef enum DirEntryType{
	FILE_TYPE=0,
	DIRECTORY_TYPE=1
} DirEntryType;

typedef enum FAT_ops_result{
    Success = 0x0,
    NoFreeBlocks = -1,
    NotExistingChild = -2,
    AlreadyExistingChild = -3,
    NoSuchDirectory = -4,
    InvalidSeekOffset = -5,
    NoFreeEntries = -6,
    NoSuchEntryInDirTable = -7,
    BackFromRootError = -8,
    TestFailed = -9
} FAT_ops_result;

typedef enum FatWhence{
    FAT_CUR=0,
    FAT_END =1,
    FAT_SET= 2
} FatWhence;


typedef struct FatEntry {
    char entry_name[MAX_NAME_LENGTH];
    uint32_t free:1;
    uint32_t eof:1;
    uint32_t directory:1;
    uint32_t next:29;
    uint32_t num_children;
    int32_t parent_idx; //only root has value -1
}FatEntry;

typedef struct FatTable {
    FatEntry entries[BLOCKS_NUM];
} FatTable;

typedef struct DirectoryEntry{
    char name[MAX_NAME_LENGTH];
    uint32_t first_fat_entry;
} DirectoryEntry;

typedef struct Block{
    DirectoryEntry directory_table[MAX_CHILDREN_NUM];
    char file_content[BLOCK_SIZE];
} Block;


typedef struct Disk{
    FatTable fat_table;
    //DirTable dir_table;
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
    uint32_t num_blocks_occupied;
} FileHandle;

//initialize a new disk which will be an mmapped file and returns a wrapper
Wrapper* fat_init(const char* filename);
//initialize fat table with alle entries set to free
int fat_table_init(Wrapper* wrapper);
// free resources
int fat_destroy(Wrapper* wrapper);

//all functions operate from current directory
FileHandle* createFile(Wrapper* wrapper, const char* filename);
FAT_ops_result eraseFile(FileHandle* file);
int32_t fat_write(FileHandle* handle, const void* buffer, size_t size) ;
int32_t fat_read(FileHandle* handle, void* buffer, size_t size);
int32_t fat_seek(FileHandle* handle, int32_t offset, FatWhence whence);
FAT_ops_result createDir(Wrapper* wrapper, const char* dirName);
FAT_ops_result eraseDir(Wrapper* wrapper,const char* dirName);
FAT_ops_result changeDir(Wrapper* wrapper,const char* newDirectory);
void listDir(Wrapper* wrapper);