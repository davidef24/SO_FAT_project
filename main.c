#include "fat.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
    Wrapper * wrapper = Fat_init("disk_file");
    if (wrapper == NULL){
        perror("init error");
        return -1;
    }
    printf("New wrapper object created\n");

    int res = createFile(wrapper, "first_file.txt");
    printf("%d\n", res);


    if(Fat_destroy(wrapper) < 0){
        perror("destroy error");
        return -1;
    }
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
