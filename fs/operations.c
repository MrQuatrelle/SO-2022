#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(const tfs_params* params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(const char* name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name.
 *   - root_inode: the root directory inode.
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(const char* name, const inode_t* root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(const char* name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t* root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t* inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        // If inode is a sym link.
        if (inode->i_node_type == T_SYM_LINK) {
            inum = tfs_lookup(inode->target, root_dir_inode);
            if (inum == -1) {
                return -1;
            }
            inode = inode_get(inum);
        }

        inode_lock(inode, READ_WRITE);

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }

        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
        inode_unlock(inode);

    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        inode_t* inode = inode_get(inum);
        inode_lock(inode, READ_WRITE);

        inode_unlock(inode);
        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(const char* target, const char* link_name) {
    if (!valid_pathname(link_name)) {
        return -1;
    }

    inode_t* root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_link: root dir inode must exist");

    // target file/directory doesn't exist
    int target_i_num = tfs_lookup(target, root_dir_inode);
    if (!(target_i_num > 0))
        return -1;

    // target is not greater than a block size.
    if (strlen(target) == (tfs_default_params().block_size + 1)) {
        return -1;
    }
    // creat link to target
    int sym_link_inum = inode_create(T_SYM_LINK);
    inode_t* sym_link_inode = inode_get(sym_link_inum);
    strcpy(sym_link_inode->target, target);

    if (add_dir_entry(root_dir_inode, link_name + 1, sym_link_inum) == -1) {
        return -1; // no space
    };
    return 0;
}

/**
 * @brief Creates a hard-link to a given file or directory.
 *
 * @param target: (full) path to the target file or directory.
 * @param link_name: (full) path of the desired hard-link.
 * @return 0 if the hard-link was created successfully.
 * @return -1 if some error occurs during the creation of the hard-link.
 */
int tfs_link(const char* target, const char* link_name) {
    if (!valid_pathname(link_name)) {
        return -1;
    }

    inode_t* root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_link: root dir inode must exist");

    int target_i_num = tfs_lookup(target, root_dir_inode);
    if (!(target_i_num > 0))
        return -1; // target file/directory doesn't exist

    if (add_dir_entry(root_dir_inode, link_name + 1, target_i_num) == -1)
        return -1; // no space in directory

    inode_t* target_inode = inode_get(target_i_num);

    if (target_inode->i_node_type == T_SYM_LINK) {
        return -1;
    }

    target_inode->hard_link_counter++;

    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t* file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, const void* buffer, size_t to_write) {
    open_file_entry_t* file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t* inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");
    inode_lock(inode, READ_WRITE);

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                inode_unlock(inode);
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void* block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    inode_unlock(inode);
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void* buffer, size_t len) {
    open_file_entry_t* file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    const inode_t* inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");
    inode_lock(inode, READ_ONLY);

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void* block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    inode_unlock(inode);
    return (ssize_t)to_read;
}

int tfs_unlink(const char* target) {
    inode_t* root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_link: root dir inode must exist");

    int target_i_num = tfs_lookup(target, root_dir_inode);

    if (!(target_i_num > 0))
        return -1; // target file/directory doesn't exist

    inode_t* inode_target = inode_get(target_i_num);
    if (inode_target->i_node_type == T_SYM_LINK) {
        clear_dir_entry(root_dir_inode, target + 1);
        inode_delete(target_i_num);
    }

    clear_dir_entry(root_dir_inode, target + 1);
    inode_delete(target_i_num);
    return 0;
}

int tfs_copy_from_external_fs(const char* source_path, const char* dest_path) {
    int fhandle = tfs_open(dest_path, TFS_O_CREAT);
    if (fhandle == -1) {
        perror(
            "tfs_copy_from_external_fs: failed to open/create tfs file: '%s'.");
        return -1;
    }

    char* buffer = malloc(sizeof(char) * (tfs_default_params().block_size));
    if (buffer == NULL) {
        tfs_close(fhandle);
        perror("tfs_copy_from_external_fs: failed to alloc memory for the "
               "buffer.");
        return -1;
    }

    FILE* extFile = fopen(source_path, "r");
    if (extFile == NULL) {
        tfs_close(fhandle);
        free(buffer);
        perror("tfs_copy_from_external_fs: failed to open external file.");
        return -1;
    }

    memset(buffer, 0, sizeof(char) * (tfs_default_params().block_size));
    size_t bytes_read =
        fread(buffer, sizeof(char), (tfs_default_params().block_size), extFile);

    if (bytes_read)
        buffer[bytes_read] = '\0';

    ssize_t bytes_wrote = tfs_write(fhandle, buffer, strlen(buffer));

    tfs_close(fhandle);
    free(buffer);
    fclose(extFile);
    return (int)bytes_wrote;
}
