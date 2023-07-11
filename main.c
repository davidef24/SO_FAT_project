#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printDirTable(Wrapper wrapper){
    printf("********************directory table***********************\n");
    Disk disk = *(wrapper.current_disk);
    for(int i=0; i<DIRECTORY_ENTRIES_NUM;i++){
        DirEntry entry = disk.dir_table.entries[i];
        if(entry.entry_name[0] != 0){
            printf("At index %d of directory table there is entry %s\n", i, entry.entry_name);
        }
        
    }
    printf("*******************************************\n");
}

void printFatTable(Wrapper wrapper){
    printf("********************fat table***********************\n");
    Disk disk = *(wrapper.current_disk);
    FatTable fat_table = disk.fat_table;
    FatEntry fat_entry;
    for(int i=0; i<BLOCKS_NUM;i++){
        fat_entry = fat_table.entries[i];
        if(fat_entry.state == BUSY_ENTRY){
            printf("index %d\t value %d\n", i, fat_entry.value);
        }
        
    }
    printf("*******************************************\n");
}

int test_exceed_child(Wrapper* wrapper){
    int32_t res = createDir(wrapper, "exceed_test");
    if(res < 0) return -1;
    res = changeDir(wrapper, "exceed_test");
    if(res < 0) return -1;
    char name[MAX_NAME_LENGTH];
    for(int i=0; i <= MAX_CHILDREN_NUM; i++){
        snprintf(name, sizeof(name), "Directory_%d", i);
        res = createDir(wrapper, name);
    }
   
    if(res < 0) return 0;
    else return 1;
}

void printAllChildren(Wrapper wrapper){
    Disk* disk = wrapper.current_disk;
    uint32_t parent_dir_idx = wrapper.current_dir;
    DirEntry* parent_entry = &(disk->dir_table.entries[parent_dir_idx]);
    if(parent_entry == NULL) return;
    printf("****************Children of %s directory*********************\n", parent_entry->entry_name);
    for(int i=0; i < MAX_CHILDREN_NUM; i++){
        uint32_t child_idx = parent_entry->children[i];
        if(child_idx != FREE_DIR_CHILD_ENTRY){
            DirEntry* child = &(disk->dir_table.entries[child_idx]);
            if(child == NULL) return;
            printf("child name: %s\t index in child list: %d\t index in directory table: %d\n", child->entry_name, i, child_idx);
        }
    }
}

int test_finish_blocks(Wrapper* wrapper){
    FileHandle* handle = createFile(wrapper, "test_finish_blocks.txt");
    if(handle == NULL) return 1;
    char buffer[BLOCK_SIZE * (BLOCKS_NUM+10)];
    memset(buffer, 'a', BLOCK_SIZE*BLOCKS_NUM);
    if(fat_write(handle, buffer, sizeof(buffer)) < 0){
        //free all blocks occupied 
        if (eraseFile(handle)) return 1;
        return 0;
    }
    else return 1;
}

int main(int argc, char* argv[]){
    const char writeTest[] = "Lessons will be held on Tuesdays 10.00 am- 12.00 am and Fridays 3.00 pm - 5.00 pm";
    const char writeTest2[] = "Today we will talk about file systems and some ways to implement it";
    //const char writeTest3[30];
    char readTest[256] = {0};
    Wrapper* wrapper = fat_init("disk_file");
    if (wrapper == NULL){
        puts("init error");
        return -1;
    }
    printf("New wrapper object created\n");

    //testing the exceed of child number 
    if(test_exceed_child(wrapper)){
        puts("Test exceed_child failed");
        return -1;
    }
    int32_t res = changeDir(wrapper, "..");
    if(res == -1) return -1;
    res = eraseDir(wrapper, "exceed_test");
    if(res == -1) return -1;


    res = createDir(wrapper, "operating_system");
    if(res == -1) return -1;
    res = changeDir(wrapper, "operating_system");
    if(res == -1) return -1;

    if(test_finish_blocks(wrapper)){
        puts("Test finish_blocks failed");
        return -1;
    }
    FileHandle* handle = createFile(wrapper, "course_introduction.txt");
    res = fat_write(handle, writeTest, sizeof(writeTest));
    if(res < 0){
        puts("fat_write error");
        return -1;
    };

    printf("[FAT_WRITE 1] Correctly wrote %d bytes\n", res);
    listDir(wrapper);

    //test fat_seek
    if((res = fat_seek(handle, -15, FAT_END)) < 0){
        puts("fat_seek error");
        return -1;
    }
    printf("[FAT_SEEK END]After fat_seek, distance from start of file is %d bytes\n", res);
    //if size of fat_read exceeds file end, read till end of file
    if((res = fat_read(handle, readTest, 10)) < 0){
        puts("fat_read error");
        return -1;
    }

    printf("[FAT_READ 1] Correctly read %d bytes. \nContent:  %s\n", res, readTest);
    if((res = fat_seek(handle, -20, FAT_CUR)) < 0){
        puts("fat_seek error");
        return -1;
    }
    printf("[FAT_SEEK CUR] After fat_seek, distance from start of file is %d bytes\n", res);
    memset(readTest, 0, sizeof(readTest));
    if((res = fat_read(handle, readTest, sizeof(readTest))) < 0){
        puts("fat_read error");
        return -1;
    }
    printf("[FAT_READ 2] Correctly read %d bytes. \nContent:  %s\n", res, readTest);

    if((res = fat_seek(handle, 10, FAT_SET)) < 0){
        puts("fat_seek error");
        return -1;
    }

    printf("[FAT_SEEK SET] After fat_seek, distance from start of file is %d bytes\n", res);
    memset(readTest, 0, sizeof(readTest));
    if((res = fat_read(handle, readTest, sizeof(readTest))) < 0){
        puts("fat_read error");
        return -1;
    }

    printf("[FAT_READ 3] Correctly read %d bytes. \nContent:  %s\n", res, readTest);

    res = createDir(wrapper, "lessons");
    if(res < 0) {
        puts("createDir error");
        return -1;
    }
    res = changeDir(wrapper, "lessons");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }
    FileHandle* handle2 = createFile(wrapper, "os_2023-03-23.txt");
    if(handle2 == NULL) {
        puts("createFile error");
        return -1;
    }
    res = fat_write(handle2, writeTest2, sizeof(writeTest2));
    if(res < 0){
        puts("fat_write error");
        return -1;
    };
    printf("[FAT WRITE 3] Correctly wrote %d bytes\n", res);

    printAllChildren(*wrapper);

    //back to root
    res = changeDir(wrapper, "..");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }
    res = changeDir(wrapper, "..");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }
    
    res = createDir(wrapper, "personal_projects");
    if(res < 0) {
        puts("createDir error");
        return -1;
    }
    
    listDir(wrapper);

    printDirTable(*wrapper);
    
    res = eraseDir(wrapper, "operating_system");
    if(res < 0){
        puts("eraseDir error");
        return -1;
    }
    listDir(wrapper);

    printDirTable(*wrapper);

    res = changeDir(wrapper, "personal_projects");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }
    FileHandle* handle3 = createFile(wrapper, "project_1.txt");
    if(handle3 == NULL){
        puts("createFile error");
        return -1;
    }

    listDir(wrapper);

    res = eraseFile(handle3);
    if(res == -1){
        puts("eraseFile error");
        return -1;
    }

    printf("After erasing file project_1.txt\n");
    listDir(wrapper);

    res = changeDir(wrapper, "..");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }

    res = eraseDir(wrapper, "personal_projects");
    if(res < 0){
        puts("eraseDir error");
        return -1;
    }

    printDirTable(*wrapper);

    if(fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    free(handle);
    free(handle2);
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
