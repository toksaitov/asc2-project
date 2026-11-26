/*
    asc_krffs: makes and reads disk images with the file system of the
    kernel.

    The disk of Almurut II has KRFFS, the file system of Rach2:
    `asc2-rach2/sys/fs/krffs.c` explains it, and `krffs_format.h` next to it
    has the numbers of the format. The tool has four commands:

        asc_krffs make IMAGE SIZE_KB [FILE...]
            Writes a new image of SIZE_KB kilobytes with the given files of
            the Mac. A file keeps its name without its directories.
        asc_krffs list IMAGE
            Prints the size and the name of every file, as `ls` of the
            machine does.
        asc_krffs get IMAGE NAME FILE
            Copies a file of the image into a file of the Mac.
        asc_krffs check IMAGE
            Prints the nodes of the image and checks that they are sound.

    An image is small, so the tool keeps all of it in memory.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../asc2-rach2/sys/fs/krffs_format.h"

#define PROGRAM_NAME "asc_krffs"

#define MAX_IMAGE_SIZE 0x40000000 // 1 GB, the largest disk that the kernel looks for

// The fields of a header that the tool works with
typedef struct node
{
    uint32_t pos;
    uint32_t type;
    uint64_t id;
    uint64_t size;
    uint64_t prev_size;
    uint64_t data_size;
} node_t;

static uint8_t *image;
static uint32_t image_size;

/* Numbers in the bytes of a header: the low byte comes first */

static uint64_t get_number(uint32_t pos, uint32_t size)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < size; ++i) {
        value |= (uint64_t) image[pos + i] << (8 * i);
    }

    return value;
}

static void put_number(uint32_t pos, uint32_t size, uint64_t value)
{
    for (uint32_t i = 0; i < size; ++i) {
        image[pos + i] = (uint8_t) (value >> (8 * i));
    }
}

/* The Nodes */

/*
    Reads the header at a position. Returns false, with a message, if no
    sound header is there.
*/
static bool read_node(uint32_t pos, node_t *node)
{
    if (image_size < KRFFS_HEADER_SIZE || pos > image_size - KRFFS_HEADER_SIZE) {
        fprintf(stderr, PROGRAM_NAME ": a header at the position %u does not fit in the image\n", (unsigned) pos);
        return false;
    }
    if (get_number(pos + KRFFS_MAGIC_OFF, 2) != KRFFS_MAGIC) {
        fprintf(stderr, PROGRAM_NAME ": the node at the position %u does not start with the magic number\n", (unsigned) pos);
        return false;
    }

    node->pos = pos;
    node->type = image[pos + KRFFS_TYPE_OFF];
    node->id = get_number(pos + KRFFS_ID_OFF, 8);
    node->size = get_number(pos + KRFFS_SIZE_OFF, 8);
    node->prev_size = get_number(pos + KRFFS_PREV_SIZE_OFF, 8);
    node->data_size = get_number(pos + KRFFS_DATA_SIZE_OFF, 8);

    if (node->type > KRFFS_RESERVED_NODE) {
        fprintf(stderr, PROGRAM_NAME ": the node at the position %u has the unknown type %u\n",
                (unsigned) pos, (unsigned) node->type);
        return false;
    }
    if (node->size < KRFFS_HEADER_SIZE || node->size > image_size - pos) {
        fprintf(stderr, PROGRAM_NAME ": the node at the position %u has a size that the image cannot hold\n", (unsigned) pos);
        return false;
    }
    if (node->data_size > node->size - KRFFS_HEADER_SIZE) {
        fprintf(stderr, PROGRAM_NAME ": the node at the position %u has more data than room\n", (unsigned) pos);
        return false;
    }

    return true;
}

static void write_node(const node_t *node)
{
    put_number(node->pos + KRFFS_MAGIC_OFF, 2, KRFFS_MAGIC);
    put_number(node->pos + KRFFS_TYPE_OFF, 1, node->type);
    put_number(node->pos + KRFFS_ID_OFF, 8, node->id);
    put_number(node->pos + KRFFS_SIZE_OFF, 8, node->size);
    put_number(node->pos + KRFFS_PREV_SIZE_OFF, 8, node->prev_size);
    put_number(node->pos + KRFFS_DATA_SIZE_OFF, 8, node->data_size);
    put_number(node->pos + KRFFS_PARENT_ID_OFF, 8, 0);
}

// Returns the position of the node after `node`. The first node comes after the last one.
static uint32_t get_next_pos(const node_t *node)
{
    uint32_t pos = node->pos + (uint32_t) node->size;

    return pos == image_size ? 0 : pos;
}

static uint64_t hash_name(const char *name)
{
    uint64_t hash = KRFFS_HASH_START;
    for (; *name != '\0'; ++name) {
        hash = ((hash << 5) + hash) ^ (uint8_t) *name;
    }

    return hash;
}

static const char *get_name(const node_t *node)
{
    return (const char *) &image[node->pos + KRFFS_NAME_OFF];
}

/*
    Finds the file with a name. Returns 1 if it is there, 0 if it is not, and
    -1 if the image is damaged.
*/
static int find_file(const char *name, node_t *node)
{
    uint32_t pos = 0;
    do {
        if (!read_node(pos, node)) {
            return -1;
        }
        if (node->type == KRFFS_RESERVED_NODE && strncmp(get_name(node), name, KRFFS_NAME_SIZE) == 0) {
            return 1;
        }

        pos = get_next_pos(node);
    } while (pos != 0);

    return 0;
}

/* The Files of the Mac */

static bool read_image(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror(PROGRAM_NAME ": failed to open the image");
        return false;
    }

    bool is_read = false;

    long size = 0;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        perror(PROGRAM_NAME ": failed to find the size of the image");
    } else if (size > MAX_IMAGE_SIZE) {
        fputs(PROGRAM_NAME ": the image is larger than 1 GB\n", stderr);
    } else if ((image = (uint8_t *) malloc(size > 0 ? (size_t) size : 1)) == NULL) {
        perror(PROGRAM_NAME ": not enough memory for the image");
    } else if (size > 0 && fread(image, (size_t) size, 1, file) != 1) {
        fputs(PROGRAM_NAME ": failed to read the image\n", stderr);
    } else {
        image_size = (uint32_t) size;
        is_read = true;
    }

    fclose(file);

    return is_read;
}

static bool write_file(const char *path, const uint8_t *bytes, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(PROGRAM_NAME ": failed to open the output file");
        return false;
    }

    bool is_written = size == 0 || fwrite(bytes, size, 1, file) == 1;
    if (fclose(file) != 0) {
        is_written = false;
    }
    if (!is_written) {
        fputs(PROGRAM_NAME ": failed to write the output file\n", stderr);
    }

    return is_written;
}

/* The Commands */

/*
    Puts a file of the Mac into the image: into the first free node that is
    large enough. The part of the node that the file does not need stays
    free, if it can hold a header.
*/
static bool add_file(const char *path)
{
    const char *name = strrchr(path, '/');
    name = name != NULL ? name + 1 : path;
    if (name[0] == '\0' || strlen(name) >= KRFFS_NAME_SIZE) {
        fprintf(stderr, PROGRAM_NAME ": %s has no name that a file of the image can have\n", path);
        return false;
    }

    node_t node;
    int found = find_file(name, &node);
    if (found != 0) {
        if (found > 0) {
            fprintf(stderr, PROGRAM_NAME ": two files have the name %s\n", name);
        }
        return false;
    }

    FILE *file = fopen(path, "rb");
    struct stat info;
    if (file == NULL || stat(path, &info) != 0) {
        fprintf(stderr, PROGRAM_NAME ": failed to open %s\n", path);
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }

    bool is_added = false;

    uint64_t data_size = (uint64_t) info.st_size;
    uint64_t need = KRFFS_HEADER_SIZE + data_size;

    uint32_t pos = 0;
    do {
        if (!read_node(pos, &node)) {
            goto end;
        }
        if (node.type == KRFFS_FREE_NODE && node.size >= need) {
            break;
        }
        pos = get_next_pos(&node);
    } while (pos != 0);
    if (node.type != KRFFS_FREE_NODE || node.size < need) {
        fprintf(stderr, PROGRAM_NAME ": the image has no room for %s\n", name);
        goto end;
    }

    uint64_t rest = node.size - need;
    if (rest >= KRFFS_HEADER_SIZE) {
        node.size = need;

        node_t free_node = { .pos = pos + (uint32_t) need, .type = KRFFS_FREE_NODE, .size = rest, .prev_size = need };
        write_node(&free_node);

        node_t next_node;
        if (!read_node(get_next_pos(&free_node), &next_node)) {
            goto end;
        }
        next_node.prev_size = rest;
        write_node(&next_node);
    }

    // The header of a file: its name, a regular file with the permissions that it has on the Mac, the user 0, no times
    node.type = KRFFS_RESERVED_NODE;
    node.id = hash_name(name);
    node.data_size = data_size;
    write_node(&node);

    memset(&image[pos + KRFFS_NAME_OFF], 0, KRFFS_SIZE_OFF - KRFFS_NAME_OFF);
    memcpy(&image[pos + KRFFS_NAME_OFF], name, strlen(name));
    put_number(pos + KRFFS_MODE_OFF, 2, KRFFS_MODE_FILE | (info.st_mode & KRFFS_MODE_PERMISSIONS));

    if (data_size > 0 && fread(&image[pos + KRFFS_HEADER_SIZE], (size_t) data_size, 1, file) != 1) {
        fprintf(stderr, PROGRAM_NAME ": failed to read %s\n", path);
        goto end;
    }

    is_added = true;

end:
    fclose(file);

    return is_added;
}

static int make_image(const char *image_path, const char *size_arg, int file_count, char *file_paths[])
{
    char *end;
    unsigned long size_kb = strtoul(size_arg, &end, 10);
    if (end == size_arg || *end != '\0' || size_kb == 0 || size_kb > MAX_IMAGE_SIZE / 1024) {
        fputs(PROGRAM_NAME ": the size is a number of kilobytes, at most 1 GB\n", stderr);
        return EXIT_FAILURE;
    }

    image_size = (uint32_t) (size_kb * 1024);
    image = (uint8_t *) calloc(image_size, 1);
    if (image == NULL) {
        perror(PROGRAM_NAME ": not enough memory for the image");
        return EXIT_FAILURE;
    }

    // An empty file system: the root, which is a header without data, and one free node with the rest of the disk
    node_t root = { .pos = 0, .type = KRFFS_ROOT_NODE, .id = hash_name(""),
                    .size = KRFFS_HEADER_SIZE, .prev_size = image_size - KRFFS_HEADER_SIZE };
    write_node(&root);
    put_number(KRFFS_MODE_OFF, 2, KRFFS_ROOT_PERMISSIONS);

    node_t free_node = { .pos = KRFFS_HEADER_SIZE, .type = KRFFS_FREE_NODE,
                         .size = image_size - KRFFS_HEADER_SIZE, .prev_size = KRFFS_HEADER_SIZE };
    write_node(&free_node);

    for (int i = 0; i < file_count; ++i) {
        if (!add_file(file_paths[i])) {
            return EXIT_FAILURE;
        }
    }

    return write_file(image_path, image, image_size) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int list_image(void)
{
    node_t node;

    uint32_t pos = 0;
    do {
        if (!read_node(pos, &node)) {
            return EXIT_FAILURE;
        }
        if (node.type == KRFFS_RESERVED_NODE) {
            printf("%8u %.255s\n", (unsigned) node.data_size, get_name(&node));
        }

        pos = get_next_pos(&node);
    } while (pos != 0);

    return EXIT_SUCCESS;
}

static int get_file(const char *name, const char *out_path)
{
    node_t node;
    int found = find_file(name, &node);
    if (found == 0) {
        fprintf(stderr, PROGRAM_NAME ": the image has no file %s\n", name);
    }
    if (found <= 0) {
        return EXIT_FAILURE;
    }

    return write_file(out_path, &image[node.pos + KRFFS_HEADER_SIZE], (size_t) node.data_size) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
    Walks the ring of nodes once and checks what the kernel counts on: the
    first node is the root and no other is; every node knows the size of the
    node before it; a file has a name with an end, the hash of that name, and
    a name that no other file has; and two free nodes never touch, because a
    free node joins its neighbors.
*/
static int check_image(void)
{
    int status = EXIT_SUCCESS;

    node_t node;
    uint64_t prev_size = 0;
    uint32_t prev_type = KRFFS_ROOT_NODE;
    uint32_t files = 0;
    uint64_t free_size = 0;

    uint32_t pos = 0;
    do {
        if (!read_node(pos, &node)) {
            return EXIT_FAILURE;
        }

        const char *name = get_name(&node);

        if ((pos == 0) != (node.type == KRFFS_ROOT_NODE)) {
            printf("! the node at %u: the root must be the first node, and the only one\n", (unsigned) pos);
            status = EXIT_FAILURE;
        }
        if (pos != 0 && node.prev_size != prev_size) {
            printf("! the node at %u: the node before it has %llu bytes, and the header says %llu\n",
                   (unsigned) pos, (unsigned long long) prev_size, (unsigned long long) node.prev_size);
            status = EXIT_FAILURE;
        }
        if (get_number(pos + KRFFS_PARENT_ID_OFF, 8) != 0) {
            printf("! the node at %u: it is in a directory, and the kernel has no directories\n", (unsigned) pos);
            status = EXIT_FAILURE;
        }

        if (node.type == KRFFS_ROOT_NODE) {
            printf("* root at %u\n", (unsigned) pos);
        } else if (node.type == KRFFS_FREE_NODE) {
            printf("- free at %u: %llu bytes\n", (unsigned) pos, (unsigned long long) node.size);
            free_size += node.size - KRFFS_HEADER_SIZE;

            if (prev_type == KRFFS_FREE_NODE) {
                printf("! the node at %u: two free nodes touch\n", (unsigned) pos);
                status = EXIT_FAILURE;
            }
        } else {
            ++files;

            if (memchr(name, '\0', KRFFS_NAME_SIZE) == NULL) {
                printf("! the node at %u: the name has no end\n", (unsigned) pos);
                status = EXIT_FAILURE;
            } else {
                printf("+ file at %u: %llu bytes of %llu, %s\n", (unsigned) pos, (unsigned long long) node.data_size,
                       (unsigned long long) (node.size - KRFFS_HEADER_SIZE), name);

                if (name[0] == '\0' || strchr(name, '/') != NULL) {
                    printf("! the node at %u: the name is not one that the kernel can find\n", (unsigned) pos);
                    status = EXIT_FAILURE;
                }
                if (node.id != hash_name(name)) {
                    printf("! the node at %u: the hash is not the one of the name\n", (unsigned) pos);
                    status = EXIT_FAILURE;
                }

                node_t first;
                if (find_file(name, &first) != 1 || first.pos != pos) {
                    printf("! the node at %u: another file has the same name\n", (unsigned) pos);
                    status = EXIT_FAILURE;
                }
            }
        }

        prev_size = node.size;
        prev_type = node.type;
        pos = get_next_pos(&node);
    } while (pos != 0);

    // The node before the root is the last one
    if (!read_node(0, &node)) {
        return EXIT_FAILURE;
    }
    if (node.prev_size != prev_size) {
        printf("! the root: the last node has %llu bytes, and the header says %llu\n",
               (unsigned long long) prev_size, (unsigned long long) node.prev_size);
        status = EXIT_FAILURE;
    }

    printf("%u files, %llu bytes free: %s\n", (unsigned) files, (unsigned long long) free_size,
           status == EXIT_SUCCESS ? "the file system is sound" : "the file system is DAMAGED");

    return status;
}

int main(int argc, char *argv[])
{
    if (argc >= 4 && strcmp(argv[1], "make") == 0) {
        return make_image(argv[2], argv[3], argc - 4, &argv[4]);
    }

    if ((argc == 3 && (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "check") == 0)) ||
        (argc == 5 && strcmp(argv[1], "get") == 0)) {
        if (!read_image(argv[2])) {
            return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "list") == 0) {
            return list_image();
        }
        if (strcmp(argv[1], "check") == 0) {
            return check_image();
        }

        return get_file(argv[3], argv[4]);
    }

    fputs("Usage:\n"
              "\t" PROGRAM_NAME " make image size_in_kb [file...]\n"
              "\t" PROGRAM_NAME " list image\n"
              "\t" PROGRAM_NAME " get image name output_file\n"
              "\t" PROGRAM_NAME " check image\n", stderr);

    return EXIT_FAILURE;
}
