/*
    KRFFS, the K&R First-Fit File System of Rach2.

    The file system is the storage allocator of the book of Kernighan and
    Ritchie on a disk. The disk is one row of nodes, each right after the
    other. A node is a header and the bytes of a file behind it. The header
    has the type of the node, the name of the file, and three sizes: of the
    whole node, of the node before it, and of the data in use. With the two
    first sizes the row can be walked in both directions, and the last node
    leads back to the first: the nodes form a ring.

    The first node is the root, the directory in which all files are. It has
    a header and no data. Every other node is a file or free space.

    * A new file takes the first free node that is large enough (the first
      fit). The part that the file does not need stays a free node.
    * A removed file becomes a free node, which joins a free node after it
      and a free node before it.
    * A file that grows takes space from a free node right after it. If there
      is none, the file moves to a free node that is large enough.

    The format on the disk is the one of the tools of Rach2, so those tools
    work on a disk of this machine. A header has 357 bytes without gaps,
    and many of its numbers start at odd positions, which the CPU of this
    machine cannot read as words. So the kernel never uses a header in
    place: it copies the fields that it needs from the disk and back, byte by
    byte. The numbers of the format have 8 bytes; the kernel wants the upper
    4 to be zeros. Rach2 has directories, links, and owners; here every file
    is in the root and belongs to the user 0.

    The functions of `disk.c` give the bytes of the disk at any position.
*/

#include "sys/fs/krffs.h"

#include <stddef.h>

#include "sys/disk.h"
#include "sys/rp.h"
#include "sys/syscall.h"
#include "sys/lib/kstring.h"
#include "sys/proc/sched.h"

#define COPY_PART_SIZE 256 // bytes that are copied at a time from one place of the disk to another

static uint32_t fs_size; // size of the file system in bytes
static bool is_busy;     // a process works with the file system

/* Numbers in the bytes of a header: the low byte comes first */

static uint32_t get_u16(const uint8_t *bytes)
{
    return (uint32_t) bytes[0] | (uint32_t) bytes[1] << 8;
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return get_u16(bytes) | get_u16(bytes + 2) << 16;
}

static void put_u16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t) value;
    bytes[1] = (uint8_t) (value >> 8);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    put_u16(bytes, value);
    put_u16(bytes + 2, value >> 16);
}

/* The Nodes */

/*
    Reads the header at a position. Returns false if the disk has failed or
    no sound header is there.
*/
static bool read_node(uint32_t pos, krffs_node_t *node)
{
    uint8_t start[KRFFS_NAME_OFF];                      // the magic number, the type, and the hash
    uint8_t sizes[KRFFS_PARENT_ID_OFF - KRFFS_SIZE_OFF]; // the three sizes

    if (pos > fs_size - KRFFS_HEADER_SIZE ||
        !read_disk(pos, start, sizeof(start)) || !read_disk(pos + KRFFS_SIZE_OFF, sizes, sizeof(sizes))) {
        return false;
    }

    if (get_u16(start + KRFFS_MAGIC_OFF) != KRFFS_MAGIC || start[KRFFS_TYPE_OFF] > KRFFS_RESERVED_NODE ||
        get_u32(sizes + 4) != 0 || get_u32(sizes + 12) != 0 || get_u32(sizes + 20) != 0) {
        return false;
    }

    node->pos = pos;
    node->type = start[KRFFS_TYPE_OFF];
    node->id = (uint64_t) get_u32(start + KRFFS_ID_OFF + 4) << 32 | get_u32(start + KRFFS_ID_OFF);
    node->size = get_u32(sizes);
    node->prev_size = get_u32(sizes + 8);
    node->data_size = get_u32(sizes + 16);

    // The node is inside the disk, and the data is inside the node
    return node->size >= KRFFS_HEADER_SIZE && node->size <= fs_size - pos &&
           node->data_size <= node->size - KRFFS_HEADER_SIZE;
}

// Writes the fields of `node` into the header at its position.
static bool write_node(const krffs_node_t *node)
{
    uint8_t start[KRFFS_NAME_OFF];
    uint8_t sizes[KRFFS_HEADER_SIZE - KRFFS_SIZE_OFF]; // the three sizes and the hash of the directory, which stays 0

    put_u16(start + KRFFS_MAGIC_OFF, KRFFS_MAGIC);
    start[KRFFS_TYPE_OFF] = (uint8_t) node->type;
    put_u32(start + KRFFS_ID_OFF, (uint32_t) node->id);
    put_u32(start + KRFFS_ID_OFF + 4, (uint32_t) (node->id >> 32));

    memset(sizes, 0, sizeof(sizes));
    put_u32(sizes, node->size);
    put_u32(sizes + 8, node->prev_size);
    put_u32(sizes + 16, node->data_size);

    return write_disk(node->pos, start, sizeof(start)) && write_disk(node->pos + KRFFS_SIZE_OFF, sizes, sizeof(sizes));
}

// Returns the position of the node after `node`. The first node comes after the last one.
static uint32_t get_next_pos(const krffs_node_t *node)
{
    uint32_t pos = node->pos + node->size;

    return pos == fs_size ? 0 : pos;
}

// Tells the node at a position the size of the node before it.
static bool set_prev_size(uint32_t pos, uint32_t prev_size)
{
    krffs_node_t node;
    if (!read_node(pos, &node)) {
        return false;
    }
    node.prev_size = prev_size;

    return write_node(&node);
}

// Makes a free node of `size` bytes at a position, after a node of `prev_size` bytes.
static bool put_free_node(uint32_t pos, uint32_t size, uint32_t prev_size)
{
    krffs_node_t node;
    node.pos = pos;
    node.type = KRFFS_FREE_NODE;
    node.id = 0;
    node.size = size;
    node.prev_size = prev_size;
    node.data_size = 0;

    return write_node(&node) && set_prev_size(get_next_pos(&node), size);
}

/* The Names and the Other Fields */

/*
    The hash of a name, as Rach2 computes it. A file is looked for by the
    hash first, which is one number to compare for every node.
*/
static uint64_t hash_name(const char *name)
{
    uint64_t hash = KRFFS_HASH_START;
    for (; *name != '\0'; ++name) {
        hash = ((hash << 5) + hash) ^ (uint8_t) *name;
    }

    return hash;
}

static bool has_name(const krffs_node_t *node, const char *name)
{
    char stored[KRFFS_NAME_SIZE];
    uint32_t size = strlen(name) + 1;

    return read_disk(node->pos + KRFFS_NAME_OFF, stored, size) && memcmp(stored, name, size) == 0;
}

static bool write_name(uint32_t pos, const char *name)
{
    char padded[KRFFS_NAME_SIZE];

    memset(padded, 0, sizeof(padded));
    memcpy(padded, name, strlen(name));

    return write_disk(pos + KRFFS_NAME_OFF, padded, sizeof(padded));
}

// Writes a time, in seconds, into one of the three time fields of a header.
static bool write_time(uint32_t pos, uint32_t field_off, uint32_t time)
{
    uint8_t bytes[KRFFS_TIME_SIZE];

    memset(bytes, 0, sizeof(bytes));
    put_u32(bytes, time);

    return write_disk(pos + field_off, bytes, sizeof(bytes));
}

static bool write_times(uint32_t pos, uint32_t time)
{
    return write_time(pos, KRFFS_ATIME_OFF, time) && write_time(pos, KRFFS_MTIME_OFF, time) &&
           write_time(pos, KRFFS_CTIME_OFF, time);
}

/* The Allocator */

/*
    Makes a file node with room for `data_size` bytes from the first free
    node that is large enough. The name and the other fields of the new node
    are for the caller to write.
*/
static int32_t alloc_node(uint32_t data_size, krffs_node_t *node)
{
    uint32_t need = KRFFS_HEADER_SIZE + data_size;

    uint32_t pos = 0;
    do {
        if (!read_node(pos, node)) {
            return -EIO;
        }

        if (node->type == KRFFS_FREE_NODE && node->size >= need) {
            // The rest stays free if it can hold a header; a smaller rest goes to the file
            uint32_t rest = node->size - need;
            if (rest >= KRFFS_HEADER_SIZE) {
                node->size = need;
                if (!put_free_node(pos + need, rest, need)) {
                    return -EIO;
                }
            }

            node->type = KRFFS_RESERVED_NODE;
            node->data_size = data_size;

            return write_node(node) ? 0 : -EIO;
        }

        pos = get_next_pos(node);
    } while (pos != 0);

    return -ENOSPC;
}

/*
    Makes a file node free space, and one node of it and the free nodes that
    touch it.
*/
static int32_t free_node(const krffs_node_t *node)
{
    uint32_t pos = node->pos;
    uint32_t size = node->size;
    uint32_t prev_size = node->prev_size;

    krffs_node_t other;

    uint32_t next_pos = get_next_pos(node);
    if (next_pos != 0) {
        if (!read_node(next_pos, &other)) {
            return -EIO;
        }
        if (other.type == KRFFS_FREE_NODE) {
            size += other.size;
        }
    }

    // A file is never the first node, so there is a node before it
    if (!read_node(pos - prev_size, &other) || other.size != prev_size) {
        return -EIO;
    }
    if (other.type == KRFFS_FREE_NODE) {
        pos = other.pos;
        size += other.size;
        prev_size = other.prev_size;
    }

    return put_free_node(pos, size, prev_size) ? 0 : -EIO;
}

// Copies bytes from one place of the disk to another. The two ranges must not overlap.
static bool copy_bytes(uint32_t dst_pos, uint32_t src_pos, uint32_t size)
{
    uint8_t part[COPY_PART_SIZE];

    while (size > 0) {
        uint32_t count = size < sizeof(part) ? size : sizeof(part);
        if (!read_disk(src_pos, part, count) || !write_disk(dst_pos, part, count)) {
            return false;
        }

        dst_pos += count;
        src_pos += count;
        size -= count;
    }

    return true;
}

static bool zero_bytes(uint32_t pos, uint32_t size)
{
    uint8_t part[COPY_PART_SIZE];

    memset(part, 0, sizeof(part));
    while (size > 0) {
        uint32_t count = size < sizeof(part) ? size : sizeof(part);
        if (!write_disk(pos, part, count)) {
            return false;
        }

        pos += count;
        size -= count;
    }

    return true;
}

/*
    Gives a file node room for exactly `data_size` bytes of data, and sets the
    size of its data to that. The data that fits is kept; new bytes have what
    the disk had there.
*/
static int32_t resize_node(krffs_node_t *node, uint32_t data_size)
{
    uint32_t need = KRFFS_HEADER_SIZE + data_size;

    krffs_node_t next;
    uint32_t next_pos = get_next_pos(node);
    if (next_pos != 0 && !read_node(next_pos, &next)) {
        return -EIO;
    }
    bool is_next_free = next_pos != 0 && next.type == KRFFS_FREE_NODE;

    if (need <= node->size) {
        // The node gets smaller. The bytes that it gives up join a free node after it, or become one
        uint32_t rest = node->size - need;
        if (is_next_free) {
            rest += next.size;
        }

        // A rest that cannot hold a header stays with the file
        if (rest >= KRFFS_HEADER_SIZE) {
            node->size = need;
        }
        node->data_size = data_size;
        if (!write_node(node) || (rest >= KRFFS_HEADER_SIZE && !put_free_node(node->pos + need, rest, need))) {
            return -EIO;
        }

        return 0;
    }

    uint32_t more = need - node->size;
    if (is_next_free && next.size >= more) {
        // The node grows into the free node after it
        uint32_t rest = next.size - more;
        if (rest < KRFFS_HEADER_SIZE) {
            more += rest;
            rest = 0;
        }

        node->size += more;
        node->data_size = data_size;
        if (!write_node(node)) {
            return -EIO;
        }
        if (rest > 0) {
            return put_free_node(node->pos + node->size, rest, node->size) ? 0 : -EIO;
        }

        return set_prev_size(get_next_pos(node), node->size) ? 0 : -EIO;
    }

    // The file moves: a new node, the name with the other fields and the data copied, and the old node freed
    krffs_node_t new_node;
    int32_t result = alloc_node(data_size, &new_node);
    if (result != 0) {
        return result;
    }

    new_node.id = node->id;
    if (!write_node(&new_node) ||
        !copy_bytes(new_node.pos + KRFFS_NAME_OFF, node->pos + KRFFS_NAME_OFF, KRFFS_SIZE_OFF - KRFFS_NAME_OFF) ||
        !copy_bytes(new_node.pos + KRFFS_HEADER_SIZE, node->pos + KRFFS_HEADER_SIZE, node->data_size)) {
        return -EIO;
    }

    // The new node may have changed the size of the node before the old one
    if (!read_node(node->pos, node)) {
        return -EIO;
    }
    result = free_node(node);
    if (result != 0) {
        return result;
    }

    // The old node may have joined the free node behind the new one, so the new header is read again too
    return read_node(new_node.pos, node) ? 0 : -EIO;
}

/* The File System */

bool init_krffs(void)
{
    fs_size = get_disk_size();
    is_busy = false;

    krffs_node_t node;
    if (fs_size < 2 * KRFFS_HEADER_SIZE || !read_node(0, &node) || node.type != KRFFS_ROOT_NODE) {
        return false;
    }

    // The nodes must lead from the root back to the root
    uint32_t pos = get_next_pos(&node);
    while (pos != 0) {
        if (!read_node(pos, &node) || node.type == KRFFS_ROOT_NODE) {
            return false;
        }
        pos = get_next_pos(&node);
    }

    return true;
}

void enter_krffs(void)
{
    while (is_busy) {
        sleep(&is_busy);
    }
    is_busy = true;
}

bool leave_krffs(void)
{
    // The sleep of a disk write is inside the visit, so the disk never has half of a change of another process
    bool is_synced = sync_disk();

    is_busy = false;
    wakeup(&is_busy);

    return is_synced;
}

int32_t krffs_get_node(uint32_t pos, krffs_node_t *node)
{
    return read_node(pos, node) ? 0 : -EIO;
}

int32_t krffs_find(const char *name, krffs_node_t *node)
{
    uint64_t id = hash_name(name);

    uint32_t pos = 0;
    do {
        if (!read_node(pos, node)) {
            return -EIO;
        }

        // Two names may have one hash, so the name itself decides
        if (node->type == KRFFS_RESERVED_NODE && node->id == id && has_name(node, name)) {
            return 0;
        }

        pos = get_next_pos(node);
    } while (pos != 0);

    return -ENOENT;
}

int32_t krffs_create(const char *name, uint32_t mode, krffs_node_t *node)
{
    int32_t result = alloc_node(0, node);
    if (result != 0) {
        return result;
    }

    // The kind of the file, its permissions, and its owner: the user 0 of the group 0
    uint8_t owner[KRFFS_ATIME_OFF - KRFFS_MODE_OFF];
    memset(owner, 0, sizeof(owner));
    put_u16(owner, KRFFS_MODE_FILE | (mode & KRFFS_MODE_PERMISSIONS));

    node->id = hash_name(name);
    if (!write_node(node) || !write_name(node->pos, name) ||
        !write_disk(node->pos + KRFFS_MODE_OFF, owner, sizeof(owner)) || !write_times(node->pos, get_uptime())) {
        return -EIO;
    }

    return 0;
}

int32_t krffs_read(const krffs_node_t *node, uint32_t offset, void *dst, uint32_t size)
{
    if (offset >= node->data_size) {
        return 0;
    }
    if (size > node->data_size - offset) {
        size = node->data_size - offset;
    }

    return read_disk(node->pos + KRFFS_HEADER_SIZE + offset, dst, size) ? (int32_t) size : -EIO;
}

int32_t krffs_write(krffs_node_t *node, uint32_t offset, const void *src, uint32_t size)
{
    // The disk is far smaller than the largest numbers, so a sum that wraps around is a request that cannot fit
    if (offset > fs_size || size > fs_size) {
        return -ENOSPC;
    }

    uint32_t old_size = node->data_size;
    if (offset + size > old_size) {
        int32_t result = resize_node(node, offset + size);
        if (result != 0) {
            return result;
        }

        // A write that starts behind the end leaves a gap, which reads as zeros
        if (offset > old_size && !zero_bytes(node->pos + KRFFS_HEADER_SIZE + old_size, offset - old_size)) {
            return -EIO;
        }
    }

    if (!write_disk(node->pos + KRFFS_HEADER_SIZE + offset, src, size) ||
        !write_time(node->pos, KRFFS_MTIME_OFF, get_uptime())) {
        return -EIO;
    }

    return (int32_t) size;
}

int32_t krffs_resize(krffs_node_t *node, uint32_t size)
{
    if (size > fs_size) {
        return -ENOSPC;
    }

    uint32_t old_size = node->data_size;
    int32_t result = resize_node(node, size);
    if (result != 0) {
        return result;
    }

    if ((size > old_size && !zero_bytes(node->pos + KRFFS_HEADER_SIZE + old_size, size - old_size)) ||
        !write_time(node->pos, KRFFS_MTIME_OFF, get_uptime())) {
        return -EIO;
    }

    return 0;
}

int32_t krffs_remove(krffs_node_t *node)
{
    return free_node(node);
}

int32_t krffs_rename(krffs_node_t *node, const char *name)
{
    node->id = hash_name(name);

    return write_node(node) && write_name(node->pos, name) ? 0 : -EIO;
}

int32_t krffs_get_name(const krffs_node_t *node, char *name)
{
    if (!read_disk(node->pos + KRFFS_NAME_OFF, name, KRFFS_NAME_SIZE)) {
        return -EIO;
    }
    name[KRFFS_NAME_SIZE - 1] = '\0';

    return 0;
}

int32_t krffs_get_info(const krffs_node_t *node, uint32_t *mode, uint32_t *time)
{
    uint8_t mode_bytes[2];
    uint8_t time_bytes[4];

    if (!read_disk(node->pos + KRFFS_MODE_OFF, mode_bytes, sizeof(mode_bytes)) ||
        !read_disk(node->pos + KRFFS_MTIME_OFF, time_bytes, sizeof(time_bytes))) {
        return -EIO;
    }

    *mode = get_u16(mode_bytes);
    *time = get_u32(time_bytes);

    return 0;
}

int32_t krffs_set_time(const krffs_node_t *node, uint32_t time)
{
    return write_times(node->pos, time) ? 0 : -EIO;
}

int32_t krffs_next(uint32_t *pos, krffs_node_t *node)
{
    // The position after the last node is the size of the file system, which no node has
    while (*pos < fs_size) {
        if (!read_node(*pos, node)) {
            return -EIO;
        }

        *pos = node->pos + node->size;
        if (node->type == KRFFS_RESERVED_NODE) {
            return 1;
        }
    }

    return 0;
}
