#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printFatTable(Wrapper wrapper){
    printf("********************fat table***********************\n");
    Disk disk = *(wrapper.current_disk);
    FatTable fat_table = disk.fat_table;
    FatEntry fat_entry;
    for(int i=0; i<BLOCKS_NUM;i++){
        fat_entry = fat_table.entries[i];
        if(!fat_entry.free){
            printf("index: %d\tname: %s\t eof: %d\tnext: %d\tnum_children: %d\tparent_idx: %d\tdirectory:  %d\n", i, fat_entry.entry_name ,fat_entry.eof, fat_entry.next, fat_entry.num_children, fat_entry.parent_idx, fat_entry.directory);
        }
        
    }
    printf("*******************************************\n");
}

FAT_ops_result test_exceed_child(Wrapper* wrapper){
    int32_t res = createDir(wrapper, "exceed_test");
    if(res < 0) return TestFailed;
    res = changeDir(wrapper, "exceed_test");
    if(res < 0) return TestFailed;
    char name[MAX_NAME_LENGTH];
    for(int i=0; i <= MAX_CHILDREN_NUM; i++){
        snprintf(name, sizeof(name), "Directory_%d", i);
        res = createDir(wrapper, name);
    }
   
    if(res < 0) return Success;
    else return TestFailed;
}

FAT_ops_result test_finish_blocks(Wrapper* wrapper){
    FileHandle* handle = createFile(wrapper, "test_finish_blocks.txt");
    if(handle == NULL) return 1;
    char buffer[BLOCK_SIZE * (BLOCKS_NUM+10)];
    memset(buffer, 'a', BLOCK_SIZE*BLOCKS_NUM);
    if(fat_write(handle, buffer, sizeof(buffer)) < 0){
        //free all blocks occupied 
        if (eraseFile(handle)) return TestFailed;
        return Success;
    }
    else return TestFailed;
}

int main(int argc, char* argv[]){
    const char writeTest[] = "Lessons will be held on Tuesdays 10.00 am- 12.00 am and Fridays 3.00 pm - 5.00 pm.";
    const char writeTest2[] = "Today we will talk about file systems and some ways to implement it.";
    const char writeTest3[] = " For any doubt you can contact me on test@example.com.";
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
    if(res < 0) return -1;

    //recursively removes all children directory created in previous test
    res = eraseDir(wrapper, "exceed_test");
    if(res < 0) {
        puts("eraseDir error");
        return -1;
    }

    printFatTable(*wrapper);
    listDir(wrapper);
    res = createDir(wrapper, "operating_system");
    if(res < 0) {
        puts("createDir error");
        return -1;
    }
    res = changeDir(wrapper, "operating_system");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }

    if(test_finish_blocks(wrapper)){
        puts("Test finish_blocks failed");
        return -1;
    }

    printFatTable(*wrapper);
    FileHandle* handle = createFile(wrapper, "course_introduction.txt");
    if(handle == NULL){
        puts("createFile error");
        return -1;
    }
    res = fat_write(handle, writeTest, sizeof(writeTest));
    if(res < 0){
        puts("fat_write error");
        return -1;
    };

    printFatTable(*wrapper);

    printf("[FAT WRITE 1] Correctly wrote %d bytes\n", res);
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
    printFatTable(*wrapper);
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
    printf("[FAT WRITE 2] Correctly wrote %d bytes\n", res);

    printFatTable(*wrapper);

    //test fat_table behavior when adding new blocks to an existing file
    FileHandle* handle3 = createFile(wrapper, "test_to_see_fat_table.txt");
    if(handle3 == NULL){
        puts("createFile error");
        return -1;
    }
    if((res = fat_seek(handle, 0, FAT_END)) < 0){
        puts("fat_seek error");
        return -1;
    }
    res = fat_write(handle, writeTest3, sizeof(writeTest3));
    if(res < 0){
        puts("fat_write error");
        return -1;
    };

    printf("[FAT WRITE 3] Correctly wrote %d bytes\n", res);

    printf("**********fat table after extending a file boundaries*****************\n");
    printFatTable(*wrapper);

    if((res = fat_seek(handle, 0, FAT_SET)) < 0){
        puts("fat_seek error");
        return -1;
    }

    memset(readTest, 0, sizeof(readTest));
    if((res = fat_read(handle, readTest, sizeof(readTest))) < 0){
        puts("fat_read error");
        return -1;
    }

    printf("[FAT_READ 4] Correctly read %d bytes. \nContent:  %s\n", res, readTest);

    printf("Let's see how fat table changes after erasing a file\n");
    res = eraseFile(handle3);
    if(res < 0){
        puts("eraseFile error");
        return -1;
    }
    
    printFatTable(*wrapper);

   // printAllChildren(*wrapper);

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
    printf("after creating personal_project dir\n");
    printFatTable(*wrapper);
    listDir(wrapper);

    //printDirTable(*wrapper);

    res = eraseDir(wrapper, "operating_system");
    if(res < 0){
        puts("eraseDir error");
        return -1;
    }

    printf("after erasing operating system dir\n");
    printFatTable(*wrapper);
    listDir(wrapper);

  //  printDirTable(*wrapper);

    res = changeDir(wrapper, "personal_projects");
    if(res < 0) {
        puts("changeDir error");
        return -1;
    }
    printFatTable(*wrapper);
    FileHandle* handle4 = createFile(wrapper, "project_1.txt");
    if(handle4 == NULL){
        puts("createFile error");
        return -1;
    }

    printFatTable(*wrapper);
    listDir(wrapper);

    res = eraseFile(handle4);
    if(res < 0){
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


    if(fat_destroy(wrapper) < 0){
        puts("destroy error");
        return -1;
    }
    free(handle);
    free(handle2);
    printf("Wrapper succesfully destroyed\n");
    return 0;
}
