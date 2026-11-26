/*
    `memmove` of the Almurut II C compiler.

    C has no instruction of the machine for the copy of a structure, so the
    compiler calls `memmove`. A program may call it too. `memory_asm.s` has
    `memset` and `memcpy`.
*/

void *memcpy(void *dst, const void *src, unsigned size);

/* A copy that is right even if the two areas overlap: when the destination
   is above the source, the bytes are copied from the last one down. */
void *memmove(void *dst, const void *src, unsigned size)
{
    unsigned char *byte_dst = dst;
    const unsigned char *byte_src = src;

    if (byte_dst <= byte_src || byte_dst >= byte_src + size) {
        return memcpy(dst, src, size);
    }

    while (size > 0) {
        --size;
        byte_dst[size] = byte_src[size];
    }

    return dst;
}
