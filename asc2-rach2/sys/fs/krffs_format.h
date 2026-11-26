/*
    The format of KRFFS on the disk: the numbers that the kernel and the tool
    that builds a disk image (`asc2-tools/krffs/`) share. `krffs.c` explains
    the file system.

    A header has no gaps between its fields, and a number has its low byte
    first.
*/

#ifndef _KRFFS_FORMAT_H_
#define _KRFFS_FORMAT_H_

#define KRFFS_MAGIC 0x4B52 // the first two bytes of every header: the letters K and R

#define KRFFS_NAME_SIZE 256 // bytes of a name in a header, with its zero byte

/* Types of nodes */

#define KRFFS_ROOT_NODE     0 // the first node of the disk: the directory of all files
#define KRFFS_FREE_NODE     1 // free space
#define KRFFS_RESERVED_NODE 2 // a file

/* Positions of the fields in a header, and their sizes in bytes */

#define KRFFS_MAGIC_OFF     0   // 2: the magic number
#define KRFFS_TYPE_OFF      2   // 1: the type
#define KRFFS_ID_OFF        3   // 8: the hash of the name
#define KRFFS_NAME_OFF      11  // 256: the name, with zeros after it
#define KRFFS_MODE_OFF      267 // 2: the kind of the file and its permissions
#define KRFFS_UID_OFF       269 // 4: the user that owns the file
#define KRFFS_GID_OFF       273 // 4: the group that owns the file
#define KRFFS_ATIME_OFF     277 // 16: the time of the last use, as seconds and nanoseconds
#define KRFFS_MTIME_OFF     293 // 16: the time of the last change of the data
#define KRFFS_CTIME_OFF     309 // 16: the time of the last change of the header
#define KRFFS_SIZE_OFF      325 // 8: the size of the node, its header and its space for data
#define KRFFS_PREV_SIZE_OFF 333 // 8: the size of the node before
#define KRFFS_DATA_SIZE_OFF 341 // 8: the size of the data in use
#define KRFFS_PARENT_ID_OFF 349 // 8: the hash of the directory of the file, 0 for the root
#define KRFFS_HEADER_SIZE   357

#define KRFFS_TIME_SIZE 16

/* Bits of the mode of a node, those of Linux */

#define KRFFS_MODE_FILE        0100000 // a regular file
#define KRFFS_MODE_DIR         0040000 // a directory
#define KRFFS_MODE_PERMISSIONS 0777    // who may read, write, and run the file

#define KRFFS_ROOT_PERMISSIONS 0755

#define KRFFS_HASH_START 5381 // the hash of the empty name

#endif
