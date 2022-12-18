#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 10

typedef struct {
    char* filename;
} thread_arg;

char *str = "BBB!";

void* thread_fn(void* arg) {
    thread_arg args = *(thread_arg*) arg;
    char buffer[5];
    int fh = tfs_open(args.filename, 0); // 0 = RO
    if (fh == -1)
        printf("probably running out of open file entries\n");
    else printf("got file entry\n");
    sleep(1); // waits for everyone to open the file
    tfs_read(fh, buffer, 5);
    if(strcmp(buffer, str)) {
        printf("not reading properly\n");
        exit(1);
    }
    tfs_close(fh);
    return NULL;
}

int main() {
    char *path_to_file = "/file";
    pthread_t tid[NUM_THREADS];

    assert(tfs_init(NULL) != -1);

    int fh = tfs_open(path_to_file, TFS_O_CREAT);
    assert(fh != -1); //file creation failed

    // failed to write to the file?
    assert(tfs_write(fh, str, strlen(str)) == strlen(str));
    
    tfs_close(fh);

    thread_arg arg = {.filename = path_to_file };

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, thread_fn, (void*) &arg);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("\033[92m Successful test.\n\033[0m");
    return 0;
}
