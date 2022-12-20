
#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 22 // max dir entries in a directory with block size 1024
char const file_contents[] = "message";

typedef struct {
    char* path_generated_file;
} thread_input;

void* thread_fn(void* in) {
    thread_input* args = (thread_input*)in;
    assert(tfs_sym_link("/target", args->path_generated_file) != -1);
    return NULL;
}

int main() {
    pthread_t tid[NUM_THREADS];

    tfs_params params = tfs_default_params();
    params.max_inode_count = 1025;
    params.max_open_files_count = 1025;

    assert(tfs_init(&params) != -1);

    int fh = tfs_open("/target", TFS_O_CREAT);
    assert(fh != -1);
    tfs_write(fh, file_contents, strlen(file_contents));
    tfs_close(fh);

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_input* input = (thread_input*)malloc(sizeof(thread_input));
        input->path_generated_file =
            malloc(strlen(file_contents) * sizeof(char));
        sprintf(input->path_generated_file, "/l%d", i);
        if (pthread_create(&tid[i], NULL, thread_fn, (void*)input) != 0)
            return 1;
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        char link_name[7];
        sprintf(link_name, "/l%d", i);
        int lh = tfs_open(link_name, 0);
        assert(lh != -1);
        char buffer[7];
        assert(tfs_read(lh, buffer, sizeof(buffer)) == strlen(buffer));
        assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);
        assert(tfs_close(lh) != -1);
    }

    assert(tfs_open("/overTheLimit", TFS_O_CREAT) == -1);

    printf("\033[92m Successful test.\n\033[0m");

    assert(tfs_destroy() != -1);
    return 0;
}
