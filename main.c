#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printDirTable(Wrapper wrapper){
    printf("************************PRINTING DIRECTORY TABLE**************************");
    Disk disk = *(wrapper.current_disk);
    for(int i=0; i<DIRECTORY_ENTRIES_NUM;i++){
        DirEntry entry = disk.dir_table.entries[i];
        if(entry.entry_name[0] != 0){
            printf("In index %d of directory table there is entry %s\n", i, entry.entry_name);
        }
        
    }
}

int main(int argc, char* argv[]){
    const char writeTest[] = "This is a test first string for fat_write method. This is a second test string for fat_write method. This is a third test string for fat_write method.";
    const char seekTest[] = "This is a test first string for fat_seek method. This is a second test string for fat_seek method. This is a third test string for fat_seek method.";
    char readTest[256] = {0};
    Wrapper * wrapper = Disk_init("disk_file");
    if (wrapper == NULL){
        perror("init error");
        return -1;
    }
    printf("New wrapper object created\n");

    int32_t res = createDir(wrapper, "courses");
    if(res == -1) return -1; 

    res = createDir(wrapper, "images");
    if(res == -1) return -1; 

    listDir(wrapper);

    //expecting error
    res= changeDir(wrapper, "games");

    res= changeDir(wrapper, "courses");

    res = createDir(wrapper, "operating system");
    if(res == -1) return -1;

    res = createDir(wrapper, "algebra");
    if(res == -1) return -1;

    res = changeDir(wrapper, "operating system");

    const char fileName1[] = "lesson1.txt";
    FileHandle* handle1= createFile(wrapper, fileName1);
    if(handle1 == NULL){
        perror("create file error");
        return -1;
    };


    listDir(wrapper);

    int32_t num_write = fat_write(handle1, writeTest, (int)sizeof(writeTest));
    if(num_write == -1){
        return -1;
    }
    printf("Just wrote %d bytes in %s\n", num_write, fileName1);

    const char fileName2[] = "lesson2.txt";
    FileHandle* handle2= createFile(wrapper, fileName2);
    if(handle2 == NULL){
        perror("create file error");
        return -1;
    };

    num_write = fat_write(handle2, seekTest, (int)sizeof(seekTest));
    if(num_write == -1){
        return -1;
    }
    printf("Just wrote %d bytes in %s\n", num_write, fileName2);

    listDir(wrapper);

    res= changeDir(wrapper, "..");

    listDir(wrapper);

    res = changeDir(wrapper, "..");

    printf("[EXPECT ERROR]  ");
    res = changeDir(wrapper, "..");

    if(fat_seek(handle2, -28, FAT_END) == -1){
        puts("fat seek error");
        return -1;
    }

    printf("Handle state after seek: \tcurrent block index: %d\t current position: %d\n", handle1->current_block_index, handle1->current_pos);

    res= fat_read(handle1, readTest, 23);
    if(res == -1){
        return -1;
    }
    printf("Just read %d bytes from %s\n", fat_read(handle1, readTest, 10), writeTest);
    printf("After fat_read, buffer contains %s--------------------\n", seekTest);

    changeDir(wrapper, "courses");
    changeDir(wrapper, "operating system");
    eraseFile(handle1);

    printf("After delete file\n");
    listDir(wrapper);

    res = eraseDir(wrapper, "newDir1");
    printf("[AFTER ERASE DIRECTORY]\n");
    listDir(wrapper);

    if(Fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    free(handle2);
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
