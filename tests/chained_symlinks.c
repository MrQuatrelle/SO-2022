#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main() {
    char *content = "content";
    char *filepath = "/f1";
    char *link1 = "/l1";
    char *link2 = "/l2";
    char buffer[8];

    tfs_init(NULL);

    int fh = tfs_open(filepath, TFS_O_CREAT);

    // we've already tested this works
    tfs_write(fh, content, strlen(content));

    tfs_close(fh);

    // create chain symlink
    tfs_sym_link(filepath, link1);
    tfs_sym_link(link1, link2);

    fh = tfs_open(link2, 0);
    tfs_read(fh, buffer, strlen(content));
    tfs_close(fh);

    assert(strcmp(content, buffer) == 0);

    printf("\033[92m Successful test.\n\033[0m");

    return 0;
}
