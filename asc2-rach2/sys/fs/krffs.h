/*
    KRFFS, the K&R First-Fit File System of Rach2: the files of the disk.
*/

#ifndef _KRFFS_H_
#define _KRFFS_H_

#include <stdbool.h>

#include "sys/fs/krffs_format.h"
#include "sys/lib/kint.h"

/*
    What the kernel keeps of the header of a node while it works with the
    node. The header itself stays on the disk.
*/
typedef struct krffs_node
{
    uint32_t pos;       // position of the node on the disk
    uint32_t type;      // one of `KRFFS_ROOT_NODE`, `KRFFS_FREE_NODE`, and `KRFFS_RESERVED_NODE`
    uint64_t id;        // hash of the name
    uint32_t size;      // bytes of the node: its header and its space for data
    uint32_t prev_size; // size of the node before it
    uint32_t data_size; // bytes of data in use
} krffs_node_t;

// Looks for a file system on the disk. Returns false if there is none.
bool init_krffs(void);

/*
    One process at a time works with the file system. A process calls
    `enter_krffs` before it uses any function below, and `leave_krffs` after
    the last one. `leave_krffs` writes what has changed to the disk; it
    returns false if the disk has failed.
*/
void enter_krffs(void);
bool leave_krffs(void);

/*
    The functions below return 0, or what their comment names, if all went
    well, and a negative number of an error otherwise: `-ENOENT` if a file is
    not there, `-ENOSPC` if the disk is full, and `-EIO` if the disk has
    failed or the file system is damaged.
*/

// Reads the node at a position of the disk.
int32_t krffs_get_node(uint32_t pos, krffs_node_t *node);

// Finds the file with a name.
int32_t krffs_find(const char *name, krffs_node_t *node);

// Makes an empty file with a name that no file has, and with the permissions of `mode`.
int32_t krffs_create(const char *name, uint32_t mode, krffs_node_t *node);

// Copies at most `size` bytes of a file, from its byte `offset`, to `dst`. Returns the number of bytes copied.
int32_t krffs_read(const krffs_node_t *node, uint32_t offset, void *dst, uint32_t size);

/*
    Copies `size` bytes from `src` into a file, from its byte `offset`. The
    file grows if it must, and it may move on the disk: `node` has the new
    position then. Returns the number of bytes copied.
*/
int32_t krffs_write(krffs_node_t *node, uint32_t offset, const void *src, uint32_t size);

// Sets the size of the data of a file. New bytes are zeros. The file may move as in `krffs_write`.
int32_t krffs_resize(krffs_node_t *node, uint32_t size);

// Removes a file.
int32_t krffs_remove(krffs_node_t *node);

// Gives a file a name that no file has.
int32_t krffs_rename(krffs_node_t *node, const char *name);

// Copies the name of a file to the `KRFFS_NAME_SIZE` bytes at `name`.
int32_t krffs_get_name(const krffs_node_t *node, char *name);

// Reads the mode of a node and the time of its last change, in seconds.
int32_t krffs_get_info(const krffs_node_t *node, uint32_t *mode, uint32_t *time);

// Sets the times of a node to `time`, in seconds.
int32_t krffs_set_time(const krffs_node_t *node, uint32_t time);

/*
    Reads the first file at the position `pos` or after it, and moves `pos`
    behind it. A walk through all files starts with the position 0. Returns
    1 if there was a file, and 0 at the end of the disk.
*/
int32_t krffs_next(uint32_t *pos, krffs_node_t *node);

#endif
