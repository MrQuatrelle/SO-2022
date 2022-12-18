#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char path_generated_file[7];
} thread_input;

void* thread_fn(void* in) {
    thread_input* args = (thread_input*)in;
    tfs_open(args->path_generated_file, TFS_O_CREAT);
    free(args);
    return NULL;
}

// FIX: This test isn't testing anything really... Good enough for sanitizing
//      but that's it.
int main() {
    pthread_t tid[1023];

    assert(tfs_init(NULL) != -1);

    for (int i = 0; i < 1023; i++) {
        thread_input* input = (thread_input*)malloc(sizeof(thread_input));
        sprintf(input->path_generated_file, "/f%d", i);
        if (pthread_create(&tid[i], NULL, thread_fn, (void*)input) != 0)
            return 1;
    }

    for (int i = 0; i < 1023; i++) {
        pthread_join(tid[i], NULL);
    }

    // if there was no race condition on the creation of 1023 files, then the
    // creation of an extra file will exceed the maximum capacity.
    assert(tfs_open("/ftest", TFS_O_CREAT) == -1);

    printf("\033[92m Successful test.\n\033[0m");

    return 0;
}
