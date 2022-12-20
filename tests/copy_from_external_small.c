#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {

    char *str_ext_file = "BBB!";
    char *path_copied_file = "/f1";
    char *path_src = "tests/file_to_copy.txt";
    char buffer[40];

    assert(tfs_init(NULL) != -1);

    int f;
    ssize_t r;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    tfs_close(f);

    // Repeat the copy to the same file
    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, 0);
    assert(f != -1);

    // Contents should be overwriten, not appended
    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    printf("\033[92m Successful test.\n\033[0m");

    return 0;
}
