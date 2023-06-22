#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]){
    const char writeTest[] = "This is a test string for fat_seek method. This is the second test string for fat_seek method. This is the third test string for fat_seek method.";
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

    res= changeDir(wrapper, "newDir4");

    res= changeDir(wrapper, "newDir2");

    const char fileName[] = "first_file.txt";
    FileHandle* handle= createFile(wrapper, fileName);
    if(handle == NULL){
        perror("create file error");
        return -1;
    };

    listDir(wrapper);

    //expecting error message
    createFile(wrapper, "first_file.txt");

    int32_t num_write = fat_write(handle, writeTest, (int)sizeof(writeTest));
    if(num_write == -1){
        return -1;
    }
    printf("Just wrote %d bytes in %s\n", num_write, fileName);

    printf("Handle state after write: \tcurrent block index: %d\t current position: %d\n", handle->current_block_index, handle->current_pos);

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

    if(Fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
