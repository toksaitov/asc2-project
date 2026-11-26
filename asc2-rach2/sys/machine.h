/*
    The numbers of the machine that the kernel uses: addresses, ports,
    vectors, and bits of registers. Their home is `asc2-emulated/asc.h`, which
    explains each of them. The build compiles the two files together, and the
    compiler refuses a name that has two values.
*/

#ifndef _MACHINE_H_
#define _MACHINE_H_

/* Memory */

#define PAGE_SIZE   0x1000
#define KERN_BASE   0x80000000
#define IVT_SIZE    0x400

#define PTE_P 0b00000001
#define PTE_W 0b00000010
#define PTE_U 0b00000100

/* CPU */

#define CPU_EXCPTS    8

#define CPU_EXCPT_INVALID_INSTR 0
#define CPU_EXCPT_PRIVILEGED    1
#define CPU_EXCPT_MISALIGNED    2
#define CPU_EXCPT_PAGE_FAULT    3
#define CPU_EXCPT_TLB_MISS      4
#define CPU_EXCPT_BUS_ERROR     5
#define CPU_EXCPT_INT           6

#define CPU_FLAG_I 0b00010000
#define CPU_FLAG_U 0b00100000

#define CPU_CTRL_P 0b00000001
#define CPU_CTRL_C 0b00010000
#define CPU_CTRL_A 0b00100000

#define CPU_DOT_ROWS 3

#define CPU_CAUSE_VEC_MASK   0x000000ff
#define CPU_CAUSE_INT_SHIFT  8
#define CPU_CAUSE_P          0x00010000
#define CPU_CAUSE_W          0x00020000
#define CPU_CAUSE_M          0x00400000
#define CPU_CAUSE_STAT_MASK  0x07000000

/* Devices on the Chip */

#define PIC_CMD_IO_PORT  0x20
#define PIC_DATA_IO_PORT 0x21

#define PIT_IRQ          0
#define PIT_CMD_IO_PORT  0x43
#define PIT_DATA_IO_PORT 0x40

/* Devices of the RP2040 */

#define RP_IRQ             2

#define RP_RAM_SIZE_ADDR   0xFF0008
#define RP_TIME_ADDR       0xFF000C
#define RP_CON_STAT_ADDR   0xFF0010
#define RP_CON_DATA_ADDR   0xFF0014
#define RP_IRQ_STAT_ADDR   0xFF0018
#define RP_IRQ_ENABLE_ADDR 0xFF001C
#define RP_DISK_SEC_ADDR   0xFF0020
#define RP_DISK_BUF_ADDR   0xFF0024
#define RP_DISK_CMD_ADDR   0xFF0028
#define RP_DISK_STAT_ADDR  0xFF002C

#define RP_CON_FLAG_INPUT  0b00000001
#define RP_CON_FLAG_OUTPUT 0b00000010

#define RP_IRQ_FLAG_CON    0b00000001
#define RP_IRQ_FLAG_DISK   0b00000010

#define RP_DISK_SEC_SIZE   0x1000
#define RP_DISK_CMD_READ   1
#define RP_DISK_CMD_WRITE  2
#define RP_DISK_FLAG_BUSY  0b00000001
#define RP_DISK_FLAG_ERROR 0b00000010

#define RP_SCR_CMD_ADDR    0xFF0040
#define RP_SCR_PIC_ADDR    0xFF0058
#define RP_SCR_PICS        4
#define RP_USER_BASE       0xFF1000

#define RP_SCR_CMD_CLEAR   2
#define RP_SCR_CMD_CONSOLE 3
#define RP_SCR_WIDTH       128
#define RP_SCR_HEIGHT      64

/* Programs */

#define ELF_MACHINE_ASC2 0xA5C2

#endif
