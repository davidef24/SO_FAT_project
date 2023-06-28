#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printDirTable(Wrapper wrapper){
    printf("********************printing directory table***********************\n");
    Disk disk = *(wrapper.current_disk);
    for(int i=0; i<DIRECTORY_ENTRIES_NUM;i++){
        DirEntry entry = disk.dir_table.entries[i];
        if(entry.entry_name[0] != 0){
            printf("In index %d of directory table there is entry %s\n", i, entry.entry_name);
        }
        
    }
    printf("*******************************************\n");
}

int test_exceed_child(Wrapper* wrapper){
    int32_t res = createDir(wrapper, "exceed_test");
    if(res == -1) return -1;
    res = changeDir(wrapper, "exceed_test");
    if(res == -1) return -1;
    char name[MAX_NAME_LENGTH];
    for(int i=0; i <= MAX_CHILDREN_NUM; i++){
        snprintf(name, sizeof(name), "Directory_%d", i);
        res = createDir(wrapper, name);
    }
   
    if(res == -1) return 0;
    else return 1;
}

int test_finish_blocks(Wrapper* wrapper){
    FileHandle* handle = createFile(wrapper, "test_finish_blocks.txt");
    if(handle == NULL) return 1;
    char buffer[BLOCK_SIZE * (BLOCKS_NUM+10)];
    memset(buffer, 'a', BLOCK_SIZE*BLOCKS_NUM);
    if(fat_write(handle, buffer, sizeof(buffer)) < 0){
        if (eraseFile(handle)) return 1;
        return 0;
    }
    else return 1;
}



int main(int argc, char* argv[]){
    const char writeTest[] = "Lessons will be held on Tuesdays 10.00 am- 12.00 am and Fridays 3.00 pm - 5.00 pm";
    const char writeTest2[] = "Today we will talk about file systems and some ways to implement it";
    char readTest[256] = {0};
    Wrapper* wrapper = fat_init("disk_file");
    if (wrapper == NULL){
        perror("init error");
        return -1;
    }
    printf("New wrapper object created\n");

    //testing the exceed of child number 
    if(test_exceed_child(wrapper)){
        puts("Test failed");
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
        puts("Test failed");
        return -1;
    }
    FileHandle* handle = createFile(wrapper, "course_introduction.txt");
    if(fat_write(handle, writeTest, sizeof(writeTest)) < 0){
        return -1;
    };

    printf("Correctly wrote %d bytes\n", res);
    listDir(wrapper);

    if((res = fat_seek(handle, 2, FAT_SET)) < 0){
        puts("fat_seek error");
        return -1;
    }

    if((res = fat_read(handle, readTest, sizeof(readTest))) < 0){
        puts("fat_read error");
        return -1;
    }


    printf("Correctly read %d bytes. \nContent is %s\n", res, readTest);

    res = createDir(wrapper, "lessons");
    if(res == -1) return -1;
    res = changeDir(wrapper, "lessons");
    if(res == -1) return -1;

    FileHandle* handle2 = createFile(wrapper, "os_2023-03-23.txt");
    if(handle2 == NULL) return -1;
    if(fat_write(handle2, writeTest2, sizeof(writeTest2)) < 0){
        puts("fat_write error");
        return -1;
    };
    printf("Correctly wrote %d bytes\n", res);


    //back to root
    res = changeDir(wrapper, "..");
    if(res == -1) return -1;
    res = changeDir(wrapper, "..");
    if(res == -1) return -1;
    
    res = createDir(wrapper, "personal_projects");
    if(res == -1) return -1;
    
    listDir(wrapper);

    printf("BEFORE DELETE ");
    printDirTable(*wrapper);
    
    res = eraseDir(wrapper, "operating_system");
    if(res == -1) return -1;
    listDir(wrapper);

    printDirTable(*wrapper);

    res = changeDir(wrapper, "personal_projects");
    if(res == -1) return -1;
    FileHandle* handle3 = createFile(wrapper, "project_1.txt");
    if(handle3 == NULL) return -1;

    listDir(wrapper);

    res = eraseFile(handle3);
    if(res == -1) return -1;

    printf("After eraseFile project_1.txt ");
    listDir(wrapper);

    if(fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    free(handle);
    free(handle2);
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
