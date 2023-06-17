#pragma once
#include <stdint.h> 

#define BLOCKS_NUM 2048
#define BLOCK_SIZE 512
#define MAX_NAME_LENGTH 64
#define DIRECTORY_ENTRIES_NUM 512
#define MAX_CHILDREN_NUM 64

typedef enum EntryType {
	FILE_TYPE,
	DIRECTORY_TYPE
} EntryType;

typedef struct DirEntry {
    char entry_name[MAX_NAME_LENGTH];
    EntryType type;
    uint32_t first_fat_entry;
    uint32_t num_children;
    uint32_t children[MAX_CHILDREN_NUM];
} DirEntry;

typedef struct FatTable {
    uint32_t entries[BLOCKS_NUM];
} FatTable;

typedef struct DirTable {
    DirEntry entries[DIRECTORY_ENTRIES_NUM];
} DirTable;

typedef struct Block{
    char block_content[BLOCK_SIZE];
} Block;

typedef struct FileHandle {
	uint32_t current_pos;
	uint32_t current_block_index;
	uint32_t directory_entry;    
} FileHandle;

typedef struct Disk{
    FatTable fat_table;
    DirTable dir_table;
    Block block_list[BLOCKS_NUM];
} Disk;
