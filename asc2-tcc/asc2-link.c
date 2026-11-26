/*
 *  Almurut II relocations for TCC
 *
 *  Modelled on i386-link.c of TCC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef TARGET_DEFS_ONLY

/* The machine number and the relocation types: the `ELF_MACHINE_ASC2` and
   `ELF_RELOC_ASC2_` constants of asc2-emulated/asc.h. Relocation records
   carry their addends (`.rela` sections). In the formulas S is the address
   of the symbol, A the addend, and P the address of the place. */
#define EM_ASC2       0xa5c2

#define R_ASC2_NONE   0
#define R_ASC2_32     1 /* 32-bit word in data: S + A */
#define R_ASC2_LO16   2 /* immediate value of `li`: the lower 16 bits of S + A */
#define R_ASC2_HI16   3 /* immediate value of `lih`: the upper 16 bits of S + A */
#define R_ASC2_REL16  4 /* immediate value of a jump or `call`: (S + A - P) / 4 */

#define EM_TCC_TARGET EM_ASC2

/* relocation type for 32 bit data relocation */
#define R_DATA_32   R_ASC2_32
#define R_DATA_PTR  R_ASC2_32

/* Types that only a dynamic linker uses. The machine has none; the
   numbers are here because TCC's linker names them. */
#define R_JMP_SLOT  5
#define R_GLOB_DAT  6
#define R_COPY      7
#define R_RELATIVE  8

#define R_NUM       9

/* where a user program starts unless the linker is told otherwise */
#define ELF_START_ADDR 0x00400000
#define ELF_PAGE_SIZE  0x1000

#define PCRELATIVE_DLLPLT 0
#define RELOCATE_DLLPLT 0

#else /* !TARGET_DEFS_ONLY */

#include "tcc.h"

/* Returns 1 for a code relocation, 0 for a data relocation. For unknown
   relocations, returns -1. */
int code_reloc (int reloc_type)
{
    switch (reloc_type) {
        case R_ASC2_32:
        case R_ASC2_LO16:
        case R_ASC2_HI16:
            return 0;

        case R_ASC2_REL16:
            return 1;
    }

    tcc_error ("Unknown relocation type: %d", reloc_type);
    return -1;
}

/* Programs are linked statically, so no relocation needs an entry in a
   global offset table or a procedure linkage table. */
int gotplt_entry_type (int reloc_type)
{
    return NO_GOTPLT_ENTRY;
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr)
{
    tcc_error("dynamic linking is not supported");
    return 0;
}

ST_FUNC void relocate_plt(TCCState *s1)
{
}

void relocate_init(Section *sr)
{
}

/* Write the value of a relocation into its place: `ptr` points at the
   place, `addr` is the address of the place, and `val` is S + A. */
void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{
    int dist;

    switch (type) {
        case R_ASC2_32:
            write32le(ptr, val);
            return;
        case R_ASC2_LO16:
            /* the immediate value is the upper half of an instruction word */
            write16le(ptr + 2, val);
            return;
        case R_ASC2_HI16:
            write16le(ptr + 2, val >> 16);
            return;
        case R_ASC2_REL16:
            dist = (int)(val - addr);
            if (dist & 3)
                tcc_error("the target of a jump or a call at 0x%x is not a multiple of 4", (unsigned)addr);
            dist /= 4;
            if (dist != (short)dist)
                tcc_error("the target of a jump or a call at 0x%x is more than 32767 instructions away", (unsigned)addr);
            write16le(ptr + 2, dist);
            return;
        default:
            tcc_error("unknown relocation type %d at 0x%x", type, (unsigned)addr);
            return;
    }
}

#endif /* !TARGET_DEFS_ONLY */
