#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_THREADS 1024

typedef struct {
    char* filename;
} thread_arg;

void* thread_fn(void* arg) {
    thread_arg args = *(thread_arg*) arg;
    int fh = tfs_open(args.filename, TFS_O_APPEND); // 0 = RO
    if (fh == -1)
        printf("probably running out of open file entries\n");
    tfs_write(fh, "1", 1);
    
    tfs_close(fh);
    return NULL;
}

int main() {
    char *path_to_file = "/file";
    pthread_t tid[NUM_THREADS];

    tfs_params params  = tfs_default_params();
    params.max_open_files_count = 3;
    params.max_inode_count = 3;
    params.max_open_files_count = 1024;

    assert(tfs_init(&params) != -1);

    int fh = tfs_open(path_to_file, TFS_O_CREAT);
    assert(fh != -1); //file creation failed
    tfs_close(fh);

    thread_arg arg = {.filename = path_to_file };

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, thread_fn, (void*) &arg);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    fh = tfs_open(path_to_file, TFS_O_APPEND);
    assert(fh != -1); //file creation failed

    ssize_t len = tfs_write(fh, "1", 1);
    printf("len = %zd\n", len);
    assert(len == 0);
    tfs_close(fh);

    printf("\033[92m Successful test.\n\033[0m");
    return 0;
}
