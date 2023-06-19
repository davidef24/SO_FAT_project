#include "fat.h"
#include <stdio.h>
#include <stdlib.h>

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
