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

    if(createFile(wrapper, "first_file.txt") == -1){
        perror("create file error");
        return -1;
    };

    listDir(wrapper);

    if(Fat_destroy(wrapper) < 0){
        perror("destroy error");
        return -1;
    }
    printf("-----------------");
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
