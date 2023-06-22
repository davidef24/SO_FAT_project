#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]){
    const char writeTest[] = "This is a test string for fat_seek method. This is the second test string for fat_seek method. This is the third test string for fat_seek method.";
    const char test2[] = "provaprovaprovaprova";
    char readTest[256] = {0};
    Wrapper * wrapper = Disk_init("disk_file");
    if (wrapper == NULL){
        perror("init error");
        return -1;
    }
    printf("New wrapper object created\n");

    int32_t res = createDir(wrapper, "newDir1");
    if(res == -1) return -1; 

    res = createDir(wrapper, "newDir2");
    if(res == -1) return -1; 

    listDir(wrapper);

    //expecting error
    res= changeDir(wrapper, "newDir4");

    res= changeDir(wrapper, "newDir1");

    res = createDir(wrapper, "newDir3");
    if(res == -1) return -1;

    res = createDir(wrapper, "newDir4");
    if(res == -1) return -1;

    res = changeDir(wrapper, "newDir4");

    const char fileName1[] = "first_file.txt";
    FileHandle* handle1= createFile(wrapper, fileName1);
    if(handle1 == NULL){
        perror("create file error");
        return -1;
    };


    listDir(wrapper);

    //expecting error message
    createFile(wrapper, "first_file.txt");

    int32_t num_write = fat_write(handle1, writeTest, (int)sizeof(writeTest));
    if(num_write == -1){
        return -1;
    }
    printf("Just wrote %d bytes in %s\n", num_write, fileName1);

    const char fileName2[] = "second_file.txt";
    FileHandle* handle2= createFile(wrapper, fileName2);
    if(handle2 == NULL){
        perror("create file error");
        return -1;
    };

    num_write = fat_write(handle2, test2, (int)sizeof(test2));
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

    res = eraseDir(wrapper, "newDir1");
    printf("[AFTER ERASE DIRECTORY] ");
    listDir(wrapper);

/*
    printf("Handle state after write: \tcurrent block index: %d\t current position: %d\n", handle1->current_block_index, handle->current_pos);


    if(fat_seek(handle, 0, FAT_SET) == -1){
        puts("fat seek error");
        return -1;
    }

    printf("Handle state after seek: \tcurrent block index: %d\t current position: %d\n", handle->current_block_index, handle->current_pos);

    res= fat_read(handle, readTest, 500);
    if(res == -1){
        return -1;
    }
    printf("Just read %d bytes from %s\n", fat_read(handle, readTest, 10), fileName);
    printf("-----after fat_read buffer contains %s--------------------\n", readTest);

    eraseFile(handle);

    printf("After delete ");
    listDir(wrapper);
*/
    if(Fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
