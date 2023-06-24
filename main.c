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

    int32_t res = createDir(wrapper, "operating_system");
    res = changeDir(wrapper, "operating_system");
    FileHandle* handle = createFile(wrapper, "course_introduction.txt");
    if((res = fat_write(handle, writeTest, sizeof(writeTest))) < 0){
        puts("fat_write error");
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
    res = changeDir(wrapper, "lessons");

    FileHandle* handle2 = createFile(wrapper, "os_2023-03-23.txt");
    if((res = fat_write(handle2, writeTest2, sizeof(writeTest2))) < 0){
        puts("fat_write error");
        return -1;
    };
    printf("Correctly wrote %d bytes\n", res);
    res = changeDir(wrapper, "..");
    

    res = changeDir(wrapper, "..");
    

    res = createDir(wrapper, "personal_projects");
    
    listDir(wrapper);

    printDirTable(*wrapper);

    res = eraseDir(wrapper, "operating_system");
    listDir(wrapper);

    printDirTable(*wrapper);

    res = changeDir(wrapper, "personal_projects");
    FileHandle* handle3 = createFile(wrapper, "project_1");

    listDir(wrapper);

    res = eraseFile(handle3);

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
