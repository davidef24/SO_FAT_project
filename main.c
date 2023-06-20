#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]){
    Wrapper * wrapper = Disk_init("disk_file");
    if (wrapper == NULL){
        perror("init error");
        return -1;
    }
    printf("-----------------");
    printf("New wrapper object created\n");

    FileHandle* handle= createFile(wrapper, "first_file.txt");
    if(handle == NULL){
        perror("create file error");
        return -1;
    };

    listDir(wrapper);

    //expecting error message
    createFile(wrapper, "first_file.txt");

    const char test[] = "This is a test string for the method fat_write and I want it to take 2 blocks and a half to see if it works";
    fat_write(handle, test, (int)sizeof(test));

    printf("Handle state after write\n");
    printf("current block index: %d\t", handle->current_block_index);
    printf("current position: %d\n", handle->current_pos);

    fat_seek(handle, -45, FAT_CURRENT);
    

    eraseFile(handle);

    listDir(wrapper);

    if(Fat_destroy(wrapper) < 0){
        perror("destroy error");
        return -1;
    }
    printf("-----------------");
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
