#ifndef ASC_H
#define ASC_H

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* Device */

#define CPU_INST_SIZE 4           // fixed size of every instruction in bytes
#define CPU_REGS      13          // number of CPU registers that an instruction can name
#define CPU_CTRL_REGS 9           // number of control registers
#define CPU_TLB_SIZE  8           // number of translations kept in the translation lookaside buffer (TLB)
#define CPU_IC_LINES  4           // lines of eight words in the instruction cache
#define CPU_DOT_ROWS  3           // rows of four cells in the array of the dot products, from 1 to 4
#define CPU_DOT_REGS  8           // number of registers of the array that an instruction can name
#define CPU_EXCPTS    8           // number of interrupt vectors reserved for CPU exceptions
#define CPU_RESET_IP  0x80800000  // address of the first instruction after reset (the start of the boot ROM seen through the kernel window)

#define CPU_EXCPT_INVALID_INSTR 0 // exception vector for an undefined instruction encoding
#define CPU_EXCPT_PRIVILEGED    1 // exception vector for a privileged instruction in user mode
#define CPU_EXCPT_MISALIGNED    2 // exception vector for an address that is not a multiple of the access size
#define CPU_EXCPT_PAGE_FAULT    3 // exception vector for a translation that is absent, refuses the access, or cannot be read
#define CPU_EXCPT_TLB_MISS      4 // exception vector for a translation missing from the TLB while the chip's own page table lookup is off
#define CPU_EXCPT_BUS_ERROR     5 // exception vector for an address of the kernel window in user mode, or an access refused on the bus
#define CPU_EXCPT_INT           6 // exception vector for the `int` instruction

#define PAGE_SIZE   0x1000        // size of a page of virtual memory in bytes
#define KERN_BASE   0x80000000    // start of the kernel-only window that maps straight to the lower half of physical memory
#define RAM_BASE    0x000000      // physical address of RAM
#define RAM_SIZE    0x30000       // amount of RAM (192 KB)
#define ROM_BASE    0x800000      // physical address of the boot ROM
#define ROM_SIZE    0x40000       // size of the boot ROM (256 KB); the bytes after a firmware image are zeros
#define RP_BASE     0xFF0000      // physical address of the device registers served by the RP2040
#define RP_SIZE     0x10000       // size of the range of the device registers
#define HIGH_BASE   0xFFFFF000    // physical address of the high page: the last page of the 32-bit physical address space, which starts with a little RAM for tests of addresses above 16 MB
#define HIGH_SIZE   0x400         // amount of RAM at the start of the high page (1 KB, which an RP2350 has left next to RAM and the image)
#define PT_ADDR_END 0x800000      // end of the physical addresses where a page table may lie: the 8 MB that hold RAM
#define IVT_SIZE    0x400         // size of the interrupt vector table at physical address 0 (256 entries, each entry is the 4-byte address of a handler)

#define PIC_CMD_IO_PORT  0x20     // interrupt controller's command IO port to configure it
#define PIC_DATA_IO_PORT 0x21     // interrupt controller's data IO port to query its state or configure it
#define PIC_LEVEL_LINES  0b00000100 // interrupt lines that are levels: such a line is pending for as long as its device holds it

#define PIT_IRQ          0        // interrupt line of the timer in PIC
#define PIT_CMD_IO_PORT  0x43     // timer's command IO port to configure it
#define PIT_DATA_IO_PORT 0x40     // timer's data IO port to query its state or configure it
#define PIT_PRESCALER    256      // number of chip clocks in one step of the timer (the emulator has no clock and steps the timer once per instruction)

#define KBDC_IRQ           1      // interrupt line of the keyboard controller in PIC
#define KBDC_DATA_IO_PORT  0x60   // keyboard controller's data port to read the scancode register
#define KBDC_STAT_IO_PORT  0x64   // keyboard controller's status port to query its state and to clear the mark of a lost byte

#define RP_IRQ             2      // interrupt line in PIC shared by all devices of the RP2040
#define RP_ID_ADDR         0xFF0000 // read-only register with the bytes "ALM2"
#define RP_SCRATCH_ADDR    0xFF0004 // read-write register with no effect, to test the bus
#define RP_RAM_SIZE_ADDR   0xFF0008 // read-only register with the amount of RAM in bytes, a multiple of the page size
#define RP_TIME_ADDR       0xFF000C // read-only register with the number of microseconds since reset
#define RP_CON_STAT_ADDR   0xFF0010 // console's status register
#define RP_CON_DATA_ADDR   0xFF0014 // console's data register: a read takes the next input byte (0x100 if there is none), a write prints a byte
#define RP_IRQ_STAT_ADDR   0xFF0018 // devices that request an interrupt now; writing 1 to the disk's bit clears it
#define RP_IRQ_ENABLE_ADDR 0xFF001C // devices that are allowed to raise the interrupt line
#define RP_DISK_SEC_ADDR   0xFF0020 // disk's sector number for the next command
#define RP_DISK_BUF_ADDR   0xFF0024 // physical address in RAM of a buffer of one sector for the next command
#define RP_DISK_CMD_ADDR   0xFF0028 // disk's command register: writing 1 reads the sector into the buffer, writing 2 writes the buffer to the sector
#define RP_DISK_STAT_ADDR  0xFF002C // disk's status register
#define RP_LOSE_ADDR       0xFF0030 // test register: a write of n makes the RP2040 do the n-th access after this one and never answer it; reads as the number of accesses left
#define RP_BTN_STAT_ADDR   0xFF0034 // read-only register of the push button: bit 0 is set while the button is down
#define RP_BTN_COUNT_ADDR  0xFF0038 // read-only register with the number of presses of the button since reset
#define RP_SCR_STAT_ADDR   0xFF003C // screen's status register
#define RP_SCR_CMD_ADDR    0xFF0040 // screen's command register: writing 1 shows the frame memory on the screen, writing 2 clears the frame memory
#define RP_SCR_FRAMES_ADDR 0xFF0044 // read-only register with the number of frames that have reached the screen since reset
#define RP_SCR_CHAR_ADDR   0xFF0048 // write-only register that draws one character into the frame memory: the character in bits 7:0, its column (0 to 20) in bits 15:8, and its row (0 to 7) in bits 23:16
#define RP_SCR_RECT_ADDR   0xFF004C // write-only register that draws a rectangle into the frame memory: its left column in bits 6:0, its top row in bits 12:7, its width less 1 in bits 20:13, its height less 1 in bits 26:21, and the way to draw in bits 28:27
#define RP_SCR_SRC_ADDR    0xFF0050 // physical address in RAM of the picture that the next write to `RP_SCR_BLIT_ADDR` draws
#define RP_SCR_BLIT_ADDR   0xFF0054 // write-only register that draws the picture into the frame memory: the column of its left edge in bits 7:0 and the row of its top edge in bits 15:8, both signed, and the way to draw in bits 17:16
#define RP_SCR_PIC_ADDR    0xFF0058 // the first of eight registers that name four pictures for user programs: two registers for a picture, the physical address in RAM of the picture and the physical address of the page where it goes on if it does not end in its first page (0 if the pages lie one after the other)
#define RP_SCR_MOVE_ADDR   0xFF0078 // write-only register that keeps a point for the next line: x in bits 15:0 and y in bits 31:16, both signed
#define RP_SCR_LINE_ADDR   0xFF007C // write-only register that draws a lit line from the point that was kept to this one, and keeps it
#define RP_SCR_PICS        4        // number of these pictures
#define RP_USER_BASE       0xFF1000 // physical address of the page of the screen and the button, the only device page that takes accesses in user mode: a kernel gives it to the program that draws
#define RP_USER_SIZE       0x1000   // size of that page
#define RP_SCR_BASE        0xFF1000 // physical address of the frame memory of the screen
#define RP_USER_SCR_STAT_ADDR   0xFF1400 // registers in the user page that work as those above: the screen's status,
#define RP_USER_SCR_CMD_ADDR    0xFF1404 // its commands "show" and "clear",
#define RP_USER_SCR_FRAMES_ADDR 0xFF1408 // the number of frames,
#define RP_USER_SCR_CHAR_ADDR   0xFF140C // a character,
#define RP_USER_SCR_RECT_ADDR   0xFF1410 // a rectangle,
#define RP_USER_SCR_BLIT_ADDR   0xFF1414 // a picture, one of the four that the kernel has named, with its number in bits 19:18,
#define RP_USER_BTN_STAT_ADDR   0xFF1418 // the level of the button,
#define RP_USER_BTN_COUNT_ADDR  0xFF141C // the number of its presses,
#define RP_USER_TIME_ADDR       0xFF1420 // the clock in microseconds,
#define RP_USER_SCR_MOVE_ADDR   0xFF1424 // and the two registers of the lines: the point,
#define RP_USER_SCR_LINE_ADDR   0xFF1428 // and the line to it
#define RP_SCR_SIZE        0x400    // size of the frame memory: 64 rows of 16 bytes, a bit per pixel
#define RP_SCR_WIDTH       128      // pixels in a row of the screen
#define RP_SCR_HEIGHT      64       // rows of the screen
#define RP_SCR_CHAR_WIDTH  6        // pixels that a character takes in a row: five of its own and an empty one
#define RP_SCR_CHAR_HEIGHT 8        // rows that a character takes: seven of its own and an empty one
#define RP_SCR_POINT_MIN   (-512)   // the points of a line lie from here
#define RP_SCR_POINT_MAX   511      // to here
#define RP_SCR_COLS        21       // characters in a row of text
#define RP_SCR_ROWS        8        // rows of text
#define RP_DISK_SEC_SIZE   0x1000 // size of a disk sector in bytes
#define RP_CON_BUFF_SIZE   256    // number of console input bytes that the RP2040 holds for the CPU
#define RP_US_PER_STEP     8      // microseconds the emulator adds to the RP2040's clock per instruction (about the time of one fetch over the bus)
#ifndef RP_SCR_STEPS
#define RP_SCR_STEPS       512    // instructions that the way of a frame to the screen takes in the emulator (about 4 ms; the real time depends on the screen)
#endif
#ifndef RP_DISK_STEPS
#define RP_DISK_STEPS      16     // instructions that a disk command takes in the emulator (a real disk takes far longer, and not the same time twice; a build may set another number to measure with)
#endif

#define FIRMWARE_TEXT_ADDR (KERN_BASE + ROM_BASE)          // address of the text section of a firmware image (the boot ROM)
#define FIRMWARE_DATA_ADDR (KERN_BASE + RAM_BASE + 0x1000) // address of the data section of a firmware image (RAM after the page with the interrupt vector table)
#define MAX_FIRMWARE_TEXT_SIZE ROM_SIZE                    // Maximum size of the text section of a firmware image
#define MAX_FIRMWARE_DATA_SIZE (RAM_SIZE - 0x1000)         // Maximum size of the data section of a firmware image

typedef struct tlb_entry
{
    bool     valid;    // the entry holds a translation
    uint32_t vpn;      // virtual page number (bits 30:12 of a virtual address in the translated half)
    uint32_t pfn;      // physical page number (bits 31:12 of the physical address)
    bool     writable; // the page may be written
    bool     user;     // the page may be accessed in user mode
} tlb_entry_t;

typedef struct cpu
{
    /* General-Purpose Registers */

    uint32_t r0; // return value from a function (low half of a 64-bit one), caller-saved
    uint32_t r1; // high half of a 64-bit return value from a function,      caller-saved
    uint32_t r2; //                                                          caller-saved
    uint32_t r3; //                                                          caller-saved
    uint32_t r4; //                                                          callee-saved
    uint32_t r5; //                                                          callee-saved
    uint32_t r6; //                                                          callee-saved
    uint32_t r7; //                                                          callee-saved

    /*
        Calling Convention:

        The caller pushes the arguments of a function onto the stack from the
        last to the first, calls the function, and removes the arguments after
        the return. Every argument takes whole 4-byte slots: one for a value
        of up to 32 bits, two for a 64-bit value, with its low half at the
        lower address, and as many as it needs for a structure. `call` leaves
        the return address in `lr`, so a function that calls another one
        saves `lr` first. After the usual entry sequence

            push lr
            push fp
            mov sp fp

        the saved `fp` is at `fp`, the return address at `fp + 4`, and the
        first argument at `fp + 8`. The stack grows toward low addresses, `sp`
        points to the last pushed word and stays a multiple of 4. A function
        that returns a structure gets the address for it as a hidden first
        argument.

        A system call is `int 0x80` with the call number in `r0` and the
        arguments in `r1` to `r5`. The result comes back in `r0`; the kernel
        preserves every other register.
    */

    /* Special-Purpose Registers */

    uint32_t fp;  // frame pointer that points to the base of the stack of a function frame, callee-saved
    uint32_t ksp; // stack pointer of kernel mode; `sp` names it while the CPU is in kernel mode
    uint32_t usp; // stack pointer of user mode; `sp` names it while the CPU is in user mode
    uint32_t lr;  // link register with the return address from a function
    uint32_t ip;  // instruction pointer that points to the instruction being executed

    /*
        Flags' Register Bits:
            XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        Flags:
            RRRRRRRRRRRRRRRRRRRRRRRRRRUIOSZC

            C (Carry flag): Set on unsigned carry or borrow; cleared otherwise
            Z (Zero flag): Set if result is zero; cleared otherwise
            S (Sign flag): Set if the result's high bit (bit 31) is set, that is, negative in two's complement; cleared otherwise
            O (Overflow flag): Set on signed overflow; cleared otherwise
            I (Interrupt flag): Set when maskable interrupts are enabled; cleared otherwise
            U (User flag): Set while the CPU is in user mode; cleared in kernel mode
            R (Reserved): Reserved for future use, reads as zero
    */
    uint32_t flags;

    /*
        Any register can be the source or the destination of a data-movement
        or ALU instruction. Reading `ip` gives the address of the instruction
        being executed. A write to `ip` leaves `ip` exactly at the written
        value, so it acts as a jump. A write to `flags` changes the C, Z, S,
        and O flags only: the I flag changes through `cli`, `sti`, and `iret`,
        and the U flag through an interrupt, an exception, and `iret`. When
        `flags` is the explicit destination, the written result takes priority
        over the flag side effects of the same instruction. `push` stores the
        value a register had before the stack pointer decrement, and `pop`
        into `sp` keeps the popped value.

        An instruction that accesses memory changes no register before its
        last memory access has succeeded. An instruction that raises an
        exception has therefore changed no register: `ip`, the destination,
        and the stack pointer are as they were, and the instruction can be
        executed again after the kernel has removed the cause.

        Two things may have happened all the same. A page table lookup that
        succeeded before the access failed has left its translation in the
        TLB. And after the answer `BUS_STAT_LINK_ERROR` nobody knows whether
        the RP2040 has done the access: a byte of the console may be taken, a
        write may be done. The kernel does not repeat such an access blindly.
    */

    /* Control Registers (kernel mode only, `cr0` to `cr7` in assembly) */

    /*
        Control Register's Bits (`cr0`):
            RRRRRRRRRRRRRRRRRRRRRRRRRRACTFSP

            P (Paging): Set when addresses in the lower half of the address space are translated through the page tables
            S (Software fill): Set when the chip's own page table lookup is off; a missing translation then raises the TLB miss exception, and the kernel loads the TLB through `cr7`
            F (Full fetch address): Set when every instruction fetch carries its full address on the bus; set by a reset
            T (Trap record protected): Set by an interrupt or exception, cleared by `iret`; while it is set, device interrupts wait and an exception stops the CPU with a double fault
            C (Cache of instructions): Set when the chip keeps the instructions that it has fetched (see "Instruction Cache" below); cleared by a reset
            A (Array): Set when a program in user mode may use the registers of the array (see "Array of the Dot Products" below); cleared by a reset
            R (Reserved): Reserved for future use, reads as zero

        Sequential Fetch:

        The RP2040 remembers where the last instruction fetch was, and it can
        serve "the next word" with no address on the bus, which saves three
        of the nine bytes of a fetch. With F clear the CPU asks for the next
        word if the fetch before this one has succeeded and was not at the
        last word of a page, and if none of these has happened since:

        * an interrupt, an exception, or `int`;
        * `iret` or any other write to `ip`, even a write of the address that
          follows (a conditional jump that is not taken writes nothing);
        * a write to a control register, whichever it is and whatever the
          value;
        * a page table lookup by the chip that has loaded a translation.

        Every other fetch carries its address. A reset sets F, so that boot
        code runs on a chip whose sequential fetch does not work. The boot
        code clears F.

        Instruction Cache:

        Every fetch is a transaction on the bus, and most transactions are
        fetches. With C set the chip keeps the words that it has fetched in
        `CPU_IC_LINES` lines of eight words. A line holds words of one block
        of 32 bytes of the address space, as the program sees it, and a word
        counts once it has been fetched. A fetch of a word that the cache
        holds asks nothing of the bus and of the translations, and the fetch
        after it carries its address. A word of another block with the same
        line number takes the line over.

        The cache is emptied by:

        * an interrupt, an exception, or `int`;
        * `iret`;
        * a write to a control register, whichever it is and whatever the
          value;
        * the RP2040, at the start of a read of the debug port.

        The chip does not notice a write to memory. A program that writes
        instructions and then runs them must pass one of these in between;
        the call of the kernel that loads a program does.
    */
    uint32_t ctrl;        // `cr0`
    uint32_t saved_ip;    // `cr1`, address at which to continue after the last interrupt or exception
    uint32_t fault_addr;  // `cr2`, address that the last memory exception could not access
    uint32_t pt_root;     // `cr3`, physical address of the top page table (bits 31:12 are kept); a write empties the TLB
    uint32_t saved_flags; // `cr4`, flags before the last interrupt or exception
    /*
        Cause Register's Bits (`cr5`):
            RRRRRSSSRMLLFUWPNNNNNNNNVVVVVVVV

            V (Vector): Number of the interrupt vector taken
            N (Number): Operand of the `int` instruction
            P (Present): Set if the translation was present and refused the access; cleared if the translation was absent
            W (Write): Set if the access was a write
            U (User): Set if the access was made in user mode
            F (Fetch): Set if the access was an instruction fetch
            L (Level): Where the page fault was found: 0 in the access itself or in the TLB, 1 in the top page table, 2 in the second page table
            M (Malformed): Set if a page table was outside the 8 MB of physical addresses that hold RAM
            S (Status): Answer of the bus to the failed access or to the failed page table read (the `BUS_STAT_` codes), 0 if the bus was not asked
            R (Reserved): Reserved for future use, reads as zero
    */
    uint32_t cause;       // `cr5`
                          // `cr6` is the stack pointer of user mode (`usp` above)
                          // `cr7` loads a translation into the TLB when it is written, and reads as zero
    /*
        Lock of the Debug Port (`cr8`):

        The debug port shows the registers of the CPU to whoever holds the
        three wires. A write of `CPU_DBG_LOCK_KEY` to `cr8` locks the port:
        it then shows that it is locked and nothing else. Every other value
        written to `cr8` changes nothing, so a stray write cannot lock the
        port. `cr8` reads as 1 while the port is locked and as 0 otherwise.

        Only a reset opens the port again, and every reset does. The lock
        hides the registers on the port and touches nothing else: the bus,
        the clock, the reset, and the STOP pin work as before.
    */
    bool dbg_locked;      // `cr8`

    /* Array of the Dot Products */

    /*
        The array is `CPU_DOT_ROWS` rows of four cells. A cell multiplies two
        signed bytes, a weight and an input, and the cells of a row add their
        products up: the step at the heart of a neural network. `dot4` gives
        one row its bytes in two registers of the CPU. The instructions below
        use all rows, with bytes that stay in the array. (The chip has the
        four cells of one row and sends the rows through them in turn.)

        A row keeps its four weights in a register of the array, `w0` for the
        first row, `w1` for the second, and so on, and its four inputs in
        `x0`, `x1`, and so on. The low byte of a register belongs to the
        first cell of the row. `mov` copies a word between a register of the
        array and a register of the CPU, in both directions. The registers of
        the array change in no other way than the ones described here.

            dotv rd     adds the products of all cells to `rd`
            dotm rd     adds the products of the first row to `rd`, of the second row to the register after `rd`, and so on
            learn ra    corrects every weight by an error times the input of its cell
            xsh ra      moves every input on by one cell and gives the first cell the low byte of `ra`

        `dotv` is one neuron with as many inputs as the array has cells.
        `rd` holds the bias before the instruction and the sum after it, and
        a sum of more inputs takes several `dotv` in a row. `dotv` also sets
        the flags as `test` does for the new value of `rd`: Z if it is zero,
        S if it is negative, C and O clear. The sign of the sum is the
        answer of a perceptron, so a conditional jump can follow at once.

        `dotm` is a neuron per row, or a matrix times a vector: a row of the
        matrix in each `w`, the same vector in each `x`. The registers that
        take the sums must be among `r0` to `r7`. The flags do not change.

        `learn` is the delta rule. The byte of `ra` with the number of the
        row is the error of that row, a signed number of 128ths. Every
        weight of the row becomes

            weight + (error * input + 64) / 128

        rounded down, and stays between -128 and 127 if the sum leaves that
        range. An error of 127 moves a weight by about its input, an error
        of -128 by exactly minus its input, and an error of 16 by an eighth
        of it. With the error of a neuron in its byte, this is also the
        correction of a layer in backpropagation.

        `xsh` makes the inputs a line of bytes that data moves along while
        the weights stay: the first cell of `x0` takes the new byte, every
        other cell the byte of the cell before it, the first cell of `x1`
        the byte of the last cell of `x0`, and the byte of the last cell of
        the last row is dropped. A filter gives the line a sample with `xsh`
        and reads an output with `dotv`. A network gives it the outputs of a
        layer one by one, and has the inputs of the next layer in place.

        The registers of the array belong to a process as the registers of
        the CPU do. While the A bit of `cr0` is clear, `mov` with a register
        of the array, `dotv`, `dotm`, `learn`, and `xsh` raise the privileged
        instruction exception in user mode. A kernel saves and restores the
        registers of the array only for a process that has met this
        exception. `dot4` uses no register of the array and is never
        privileged.

        A reset does not prepare the registers of the array: a program writes
        a register before it uses it.
    */
    uint32_t dot_w[CPU_DOT_ROWS]; // `w0`, `w1`, ...: the weights of a row
    uint32_t dot_x[CPU_DOT_ROWS]; // `x0`, `x1`, ...: the inputs of a row

    /* Translation Lookaside Buffer */

    tlb_entry_t tlb[CPU_TLB_SIZE]; // the translations that the CPU remembers
    uint8_t tlb_next;              // index of the entry that the next new translation replaces

    /* Execution State */

    bool halted;       // the CPU has executed `hlt` and waits for an interrupt
    bool fetch_run;    // the RP2040 can serve the next fetch with no address (see "Sequential Fetch" above)

    /* Instruction Cache (see "Instruction Cache" above) */

    uint32_t ic_tag[CPU_IC_LINES];       // number of the block of 32 bytes that a line holds words of
    uint8_t ic_valid[CPU_IC_LINES];      // a bit for each word of a line that has been fetched
    uint32_t ic_data[CPU_IC_LINES * 8];  // the words
    bool double_fault; // the CPU has stopped on an exception raised while the trap record was protected; only a reset leaves this state

    /* Utility Fields (not part of a real device) */

    size_t cycles;    // Number of cycles since the start of the emulation
    uint32_t next_ip; // value of `ip` after the current instruction
    bool trapped;     // the current instruction has raised an exception
    bool retired;     // the last step has completed an instruction
    uint32_t inst;    // the last instruction word fetched
} cpu_t;

/*
    Address Space:

    Every address is 32 bits wide, virtual and physical, and every access is
    checked in this order.

    1. The address must be a multiple of the access size (1, 2, or 4 bytes), so
       no access spans two pages. Otherwise the CPU raises the misaligned
       exception.
    2. Addresses 0x80000000 to 0xFFFFFFFF are the kernel window: in kernel mode
       the low 31 bits are the physical address, in both states of paging. The
       kernel, its stack, and the handlers live here, so they need no page
       tables. In user mode such an address raises the bus error exception
       before anything is sent on the bus.
    3. With paging off, addresses 0x00000000 to 0x7FFFFFFF are physical
       addresses in both modes.
    4. With paging on, addresses 0x00000000 to 0x7FFFFFFF are translated in
       both modes: bits 31:12 select a page, and bits 11:0 pass through.

    The upper half of physical memory, from 0x80000000, is reached through a
    translation only. The RP2040 answers for every physical address, with a
    bus error where it serves nothing.

    Translation:

    The CPU first compares the page number with all TLB entries. On a hit the
    entry gives the physical page and the permissions with no memory access.
    On a miss the CPU reads two page table entries itself, as x86 does: the
    entry of the top table at `cr3 + 4 * (bits 31:22 of the address)`, and the
    entry of the second table at `(page named by the first entry) + 4 * (bits
    21:12 of the address)`. A page table is one page of 1024 entries.

    Page Table Entry's Bits:
            NNNNNNNNNNNNNNNNNNNNIIIIIIIIIUWP

            P (Present): Set if the entry is in use; every other bit of an absent entry is ignored
            W (Writable): Set if writes are allowed
            U (User): Set if accesses in user mode are allowed
            I (Ignored): Free for the kernel
            N (Number): Physical page number

    An access needs the permission in both entries. User mode needs U, a write
    needs W in both modes, and kernel mode does not need U. A lookup that
    succeeds replaces the TLB entry named by `tlb_next` and advances
    `tlb_next`; a lookup that fails changes nothing. The CPU never writes a
    page table.

    A write to `cr3` empties the TLB and sets `tlb_next` to 0, and so does a
    write to `cr0` that changes P or S. There is no other way to remove a
    translation, so the kernel writes `cr3` after it changes a page table
    entry that may be in the TLB.

    With the chip's own lookup off (S in `cr0`), a miss raises the TLB miss
    exception. The kernel finds the translation in software and writes a value
    with the layout of a page table entry to `cr7`: the page number of the
    address in `cr2` then translates to the page N with the permissions W and
    U. A translation of the same page that is already in the TLB is replaced
    in place. The Z bits of the value are ignored. A value without P is not a
    translation, and a write of it changes nothing; so does a write while
    `cr2` holds an address outside the translated half. `cr7` works in the
    same way while the chip's own lookup is on.
*/

#define PTE_P 0b00000001 // page table entry: Present
#define PTE_W 0b00000010 // page table entry: Writable
#define PTE_U 0b00000100 // page table entry: User

/*
    Interrupts and Exceptions:

    The interrupt vector table starts at physical address 0. Entry `v` is the
    4-byte address of the handler of vector `v`. Vectors 0 to 7 belong to the
    CPU exceptions (`CPU_EXCPT_` constants), and a device interrupt on line
    `n` of the interrupt controller takes the vector `vec_off + n`.

    The CPU enters a handler in the same way for an interrupt, an exception,
    and `int`, and it writes no memory on the way:

    1. `cr1` gets the address at which to continue: the instruction itself for
       an exception, so that it runs again; the next instruction for `int`;
       the instruction that has not started yet for a device interrupt.
    2. `cr4` gets the flags, `cr5` the cause, and for a memory exception `cr2`
       the address that could not be accessed.
    3. The U and I flags are cleared, so the handler runs in kernel mode on
       the kernel's stack pointer with interrupts disabled, and T in `cr0` is
       set.
    4. The CPU reads the handler's address from the table and continues there.

    `cr1`, `cr2`, `cr4`, and `cr5` form the trap record. A handler that may be
    interrupted itself saves the record on its stack and then clears T. `iret`
    loads `ip` from `cr1` and the flags from `cr4`, including I and U, and
    clears T.

    An exception while T is set would overwrite a record that nobody has
    saved. The CPU keeps the record and stops instead (a double fault), and
    so it does when it cannot read the handler's address from the table.

    Device time moves once per instruction: after an instruction the devices
    step, and a pending interrupt is taken before the next instruction starts.

    Order of Events:

    The clock of the chip is finer than an instruction, and two events can
    fall on the same clock. The hardware settles each case in one way:

    * The CPU takes an interrupt only between two instructions, also while
      it is halted. It takes none between the bus accesses of an instruction
      or of a page table lookup, and none while I is clear or T is set. An
      instruction that sets I (`sti` or `iret`) is over first; a request
      that waits is taken right after it.
    * The interrupt controller remembers one request per line. A second
      request on a line whose first one still waits adds nothing. A request
      that arrives in the clock in which the CPU takes the earlier one of
      the same line stays and is delivered next.
    * Of several lines that wait, the one with the lowest number is taken.
    * The line of the RP2040 is a level: it requests for as long as it is
      high, and the interrupt controller does not remember it.
    * The timer runs from the reset with the longest period, 65536 steps of
      `PIT_PRESCALER` clocks each. A request that it makes while its line is
      masked is delivered as soon as the kernel opens the line.
    * A write of the timer's period leaves a request that waits. A write of
      the low byte takes the place of the timer's step in that clock: the
      timer counts from the new period, and the old count makes no request.
    * The keyboard controller's cases are described at its status register.

    `hlt` waits for the next interrupt that the CPU can take. A request that
    arrives between a `sti` and the `hlt` after it is taken before the `hlt`,
    and the `hlt` then waits for another one. A kernel that sleeps until one
    event has happened keeps a second interrupt coming, such as the timer's,
    and checks in a loop whether the event has happened.
*/

typedef struct mem
{
    uint8_t ram[RAM_SIZE]; // RAM bytes with indices representing physical addresses from `RAM_BASE`.
    uint8_t rom[ROM_SIZE]; // boot ROM bytes with indices representing physical addresses from `ROM_BASE`.
    uint8_t high[HIGH_SIZE]; // bytes of the RAM in the high page with indices representing physical addresses from `HIGH_BASE`.
} mem_t;

typedef enum pic_state
{
    PIC_AWAITS_IMR,
    PIC_AWAITS_VEC_OFF
} pic_state_t;

typedef struct pic
{
    uint8_t imr; // mask that controls which interrupt request lines are disabled (1) and which are enabled (0)
    uint8_t irr; // request register with one pending bit per interrupt line, latched regardless of the mask
    uint8_t vec_off; // offset of the interrupt vector to use to trigger interrupt requests

    /* Utility Fields (may not be a part of a real device) */

    pic_state_t state; // IO state of the PIC
} pic_t;

typedef enum pit_state
{
    PIT_AWAITS_FREQ_DIV_HIGH,
    PIT_AWAITS_FREQ_DIV_LOW
} pit_state_t;

typedef struct pit
{
    uint8_t freq_div_high; // 16-bit timer's frequency divisor that controls how often the timer should trigger an interrupt
    uint8_t freq_div_low;
    uint8_t cnt_high;      // 16-bit timer's current counter value
    uint8_t cnt_low;

    /* Utility Fields (may not be a part of a real device) */

    pit_state_t state; // IO state of the PIT
} pit_t;

typedef struct kbdc
{
    uint8_t scancode; // the last byte received from the keyboard

    /*
        Status' Register Bits:
            XXXXXXXX
        Status:
            RRRRRROW

            W (Waiting): Set when a byte has arrived and has not been read yet; cleared by a read of the data port
            O (Overrun): Set when a byte has replaced one that was not read; cleared by a write to the status port with this bit set
            R (Reserved): Reserved for future use

        A read of the data port leaves O alone: a driver that reads the status
        and then the data would otherwise never learn of a byte that was lost
        between its two reads.

        The hardware settles two events in the same clock so that nothing is
        lost. A byte that arrives while the data port is read: the read
        returns the old byte, and the new one waits. A byte that is lost
        while O is cleared: O stays set.
    */
    uint8_t status;
} kbdc_t;

/*
    The RP2040 microcontroller next to the chip answers every access that the
    CPU makes on its bus. It serves RAM, the boot ROM, and the registers of
    its own devices: a console, a disk, a clock, a push button, and a small
    screen. The device registers are memory mapped at `RP_BASE`, take 4-byte
    accesses in kernel mode only, and share one interrupt line. One page of
    them, with the screen and the button, takes accesses in user mode too.
*/
typedef struct rp
{
    uint32_t scratch; // value of the scratch register
    uint32_t time_us; // microseconds since reset

    /*
        Console's Status Register Bits:
            RRRRRRRRRRRRRRRRRRRRRRRRRRRRRROI

            I (Input): Set when an input byte is waiting in the data register
            O (Output): Set when the console can take an output byte
            R (Reserved): Reserved for future use
    */
    uint8_t con_in[RP_CON_BUFF_SIZE]; // queue of input bytes that the CPU has not taken yet
    size_t con_in_pos;                // index of the oldest byte in the queue
    size_t con_in_cnt;                // number of bytes in the queue

    /*
        Interrupt Status and Interrupt Enable Registers' Bits:
            RRRRRRRRRRRRRRRRRRRRRRRRRRRRRBDC

            C (Console): A console input byte is waiting; drops when the last byte is taken
            D (Disk): A disk command has finished; drops when 1 is written to this bit of the status register
            B (Button): The button was pressed; drops when 1 is written to this bit of the status register
            R (Reserved): Reserved for future use

        The interrupt line is high while a device requests an interrupt and is
        enabled.
    */
    uint32_t irq_stat;   // the D and B bits are stored; C follows the input queue
    uint32_t irq_enable;

    /*
        Disk's Status Register Bits:
            RRRRRRRRRRRRRRRRRRRRRRRRRRRRRREB

            B (Busy): Set while a command is running; the buffer must not be touched then, and writes to the disk's registers are ignored
            E (Error): Set if the last command named a sector outside the disk or a buffer outside RAM
            R (Reserved): Reserved for future use
    */
    uint32_t disk_sec;  // sector number for the next command
    uint32_t disk_buf;  // physical address of the buffer for the next command
    uint32_t disk_cmd;  // command that is running (0 if none)
    uint32_t disk_stat;

    uint32_t fetch_addr; // physical address after the last instruction fetch, where a fetch of "the next word" reads
    bool fetch_valid;    // there was such a fetch, and it has succeeded
    uint32_t lose_cnt;   // number of accesses until the one whose answer gets lost (0 if none is planned)

    /*
        The push button. It has one register for its level and one that
        counts the presses, so that a program that looks seldom misses none.
    */
    bool btn_down;      // the button is down
    uint32_t btn_count; // number of presses since reset

    /*
        The screen has 128 by 64 pixels, each on or off. A program draws into
        the frame memory and then writes the command "show": the RP2040 takes
        a copy of the frame memory and sends it to the screen, which takes a
        while. The program may draw the next frame meanwhile.

        Frame memory: the pixel in column x of row y is bit x % 8 of the byte
        at y * 16 + x / 8, so the leftmost pixel of a word is its bit 0.

        Screen's Status Register Bits:
            RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRB

            B (Busy): Set while a frame is on its way to the screen; the command "show" is ignored then
            R (Reserved): Reserved for future use

        Two registers draw for the program, because the CPU reaches the frame
        memory only one word at a time over the bus. One draws a rectangle.
        The other draws a picture that lies in RAM: a word with the width in
        its bits 15:0 and the height in its bits 31:16, then the rows from
        the top, each in as many words as its width needs, the leftmost
        pixel in bit 0 of the first word. A picture is at most as large as
        the screen, and the parts of a rectangle or of a picture that lie
        outside the screen are left out. For a picture the ways "dark",
        "light", and "flip" change only the pixels under its lit pixels.

        The screen is the console's until a program draws. While it is the
        console's, the RP2040 draws every byte that the CPU prints into the
        frame memory as a terminal of 8 rows of 21 characters does, with a
        cursor, and shows the frame by itself. The first write of a program
        to the frame memory or to a register that draws or shows takes the
        screen from the console and clears the frame memory. The command
        "console" gives the screen back and clears it; a kernel writes it
        when the program that drew has ended.

        A user program has its pictures at addresses that the page tables
        translate, and a picture may lie in two pages that are not next to
        each other in RAM. So a kernel names up to four pictures for the
        program, each with the physical address of its start and of the page
        where it goes on, and the program draws one by its number.
    */
    uint8_t scr[RP_SCR_SIZE]; // frame memory
    uint32_t scr_point;       // the point that the next line starts at, as `RP_SCR_MOVE_ADDR` takes it
    uint32_t scr_src;         // physical address of the picture for the next write to `RP_SCR_BLIT_ADDR`
    uint32_t scr_pic[RP_SCR_PICS][2]; // the pictures for user programs: the address of the start, and of the page where it goes on
    bool scr_console;         // the screen is the console's
    bool scr_dirty;           // the console has drawn since the frame was last shown
    uint32_t con_row;         // place of the console's cursor
    uint32_t con_col;
    uint32_t con_esc;         // the console is inside an escape sequence: 1 after the escape byte, 2 after the bracket
    uint32_t scr_stat;
    uint32_t scr_frames;      // number of frames that have reached the screen

    /* Utility Fields (not part of a real device) */

    uint8_t scr_sent[RP_SCR_SIZE];  // the frame that is on its way to the screen
    uint8_t scr_shown[RP_SCR_SIZE]; // what the screen shows
    size_t scr_steps;               // steps left until the frame on its way arrives

    uint8_t con_out;     // byte that the CPU has printed during the current step
    bool con_out_valid;  // the `con_out` field holds a byte to show
    uint8_t *disk;       // contents of the disk (owned by the program that runs the emulator)
    size_t disk_size;    // size of the disk in bytes
    size_t disk_steps;   // steps left until the running disk command ends
} rp_t;

typedef enum mem_access
{
    MEM_READ,  // a data read
    MEM_WRITE, // a data write
    MEM_FETCH  // an instruction fetch
} mem_access_t;

/*
    One request of the CPU on the bus with its answer. The emulator keeps the
    requests of a step, so that a tool can compare them with the requests of
    the chip.
*/
typedef struct bus_req
{
    mem_access_t access;
    bool next;     // a fetch of "the next word", which carries no address
    bool kernel;   // the CPU was in kernel mode
    uint8_t size;  // number of bytes: 1, 2, or 4
    uint32_t addr; // physical address; for a fetch of "the next word", the address that the RP2040 has read
    uint32_t data; // the `size` bytes written, or the bytes read (0 if the read has failed)
    uint8_t stat;  // answer (`BUS_STAT_` codes)
} bus_req_t;

#define BUS_REQS_MAX 8 // the most requests between two steps of the CPU: two fetches of page table entries and one access, for the instruction and for its data, and the address of a handler twice

typedef struct device
{
    /* On the chip */

    cpu_t  cpu;
    pic_t  pic;
    pit_t  pit;
    kbdc_t kbdc;

    /* Served by the RP2040 */

    mem_t  mem;
    rp_t   rp;

    /* Utility Fields (not part of a real device) */

    bus_req_t bus_reqs[BUS_REQS_MAX]; // requests that the CPU has sent on the bus since the start of its last step
    size_t bus_reqs_cnt;
} device_t;

/* Instruction Encoding */

/*
    Every instruction is one 32-bit word stored with its low byte first:

        bits  7:0   opcode
        bits 11:8   register B, the second operand in assembly
        bits 15:12  register A, the first operand in assembly
        bits 31:16  16-bit immediate value

    In a memory dump the four bytes of `add r1 r2` are `20 12 00 00`: the
    opcode, the two registers in the order of the assembly line, and the
    immediate value.

    Assembly lists the source first and the destination last, with no commas.

    Bit 7 of the opcode selects the immediate form of an instruction: the
    first operand is then the immediate value, sign-extended from 16 to 32
    bits, in place of register A. `add r1 r2` adds two registers, and
    `add 0x4 r2` adds 4 to `r2`. The assembler picks the form by the kind of
    the first operand. For the jumps and `call`, the immediate form names a
    target relative to the jump itself: `ip + 4 * immediate`.

    A field that an instruction does not use must be zero. Every other word,
    including a register number of 13 or above, raises the invalid
    instruction exception.
*/

#define CPU_OPC_IMM 0x80 // bit of the opcode that selects the immediate form of an instruction

typedef enum cpu_mnem_code
{
    /* System Instructions */

    CPU_MNEM_HLT  = 0x00, // halts the CPU until the next interrupt (privileged)
    CPU_MNEM_NOP  = 0x01, // does nothing
    CPU_MNEM_CLI  = 0x02, // clears the interrupt flag (privileged)
    CPU_MNEM_STI  = 0x03, // sets the interrupt flag (privileged)
    CPU_MNEM_IRET = 0x04, // returns from an interrupt or exception restoring `ip` and `flags` from the trap record (privileged)
    CPU_MNEM_INT  = 0x05, // enters the kernel through the vector `CPU_EXCPT_INT` with a number from 0 to 255 for the kernel to read (immediate form only)
    CPU_MNEM_INB  = 0x06, // reads a byte from a port into a register (privileged)
    CPU_MNEM_OUTB = 0x07, // writes a byte from a register into a port (privileged)
    CPU_MNEM_MFC  = 0x08, // moves a value from a control register to a register, written as `mov` (privileged)
    CPU_MNEM_MTC  = 0x09, // moves a value from a register to a control register, written as `mov` (privileged)

    /* Load and Store Instructions */

    CPU_MNEM_MOV  = 0x10, // moves a value from one register to another; in the immediate form, written as `li`, loads a sign-extended immediate value into a register
    CPU_MNEM_LIH  = 0x11, // loads an immediate value into the upper 16 bits of a register and keeps the lower 16 bits (immediate form only)
    CPU_MNEM_PUSH = 0x12, // pushes a value from a register onto the stack
    CPU_MNEM_POP  = 0x13, // pops a value from the stack into a register
    CPU_MNEM_SR   = 0x14, // stores a 4-byte value from a register into a memory address
    CPU_MNEM_SRH  = 0x15, // stores the low 2 bytes of a register into a memory address
    CPU_MNEM_SRB  = 0x16, // stores the low byte of a register into a memory address
    CPU_MNEM_LR   = 0x18, // loads a 4-byte value from a memory address into a register
    CPU_MNEM_LRH  = 0x19, // loads a 2-byte value from a memory address into a register extending its sign
    CPU_MNEM_LRB  = 0x1A, // loads a byte from a memory address into a register extending its sign
    CPU_MNEM_LRHU = 0x1D, // loads a 2-byte value from a memory address into a register filling the upper bits with zeros
    CPU_MNEM_LRBU = 0x1E, // loads a byte from a memory address into a register filling the upper bits with zeros

    /* Arithmetic and Logic Instructions */

    CPU_MNEM_ADD  = 0x20, // adds two values and stores the result in a register updating flags
    CPU_MNEM_ADC  = 0x21, // adds two values with carry and stores the result in a register updating flags
    CPU_MNEM_SUB  = 0x22, // subtracts two values and stores the result in a register updating flags
    CPU_MNEM_SBB  = 0x23, // subtracts two values with borrow and stores the result in a register updating flags
    CPU_MNEM_CMP  = 0x24, // subtracts two values and updates flags
    CPU_MNEM_AND  = 0x25, // bitwise ANDs two values and stores the result in a register updating flags
    CPU_MNEM_TEST = 0x26, // bitwise ANDs two values and updates flags
    CPU_MNEM_OR   = 0x27, // bitwise ORs two values and stores the result in a register updating flags
    CPU_MNEM_XOR  = 0x28, // bitwise XORs two values and stores the result in a register updating flags
    CPU_MNEM_NOT  = 0x29, // inverts a value and stores the result in a register
    CPU_MNEM_SHL  = 0x2A, // shifts a value left and stores the result in a register updating flags
    CPU_MNEM_SHR  = 0x2B, // shifts a value right filling with zeros and stores the result in a register updating flags
    CPU_MNEM_SAR  = 0x2C, // shifts a value right filling with the sign bit and stores the result in a register updating flags
    CPU_MNEM_MUL  = 0x2D, // multiplies two values and stores the low 32 bits of the product in a register
    CPU_MNEM_DOT4 = 0x2E, // multiplies the four signed bytes of one register by the four signed bytes of another and adds the four products to a third register
    CPU_MNEM_POPCNT = 0x2F, // counts the bits of a value that are set and stores the count in a register

    /* Jump and Subroutine Control Instructions */

    CPU_MNEM_JMP  = 0x30, // jumps to a memory address
    CPU_MNEM_CALL = 0x31, // calls a function at a memory address leaving the return address in `lr`
    CPU_MNEM_RET  = 0x32, // returns from a function to the address in `lr`
    CPU_MNEM_JCC  = 0x40, // jumps to a memory address if a condition holds; the low 4 bits of the opcode select the condition (`je`, `jne`, ...)
    CPU_MNEM_SETCC = 0x50, // stores 1 in a register if a condition holds and 0 otherwise; the low 4 bits of the opcode select the condition (`sete`, `setne`, ...)

    /* Instructions of the Array of the Dot Products */

    CPU_MNEM_MFA   = 0x60, // moves a value from a register of the array to a register, written as `mov`
    CPU_MNEM_MTA   = 0x61, // moves a value from a register to a register of the array, written as `mov`
    CPU_MNEM_DOTV  = 0x62, // adds the products of all cells of the array to a register and sets the flags by the sum
    CPU_MNEM_DOTM  = 0x63, // adds the products of each row of the array to a register of its own
    CPU_MNEM_LEARN = 0x64, // corrects every weight of the array by an error times the input of its cell
    CPU_MNEM_XSH   = 0x65, // moves the inputs of the array on by one cell and takes the low byte of a register as the first
    CPU_MNEM_SSAT  = 0x66, // holds a value between -128 and 127, the range of a signed byte
    CPU_MNEM_USAT  = 0x67  // holds a value between 0 and 127, the range of a signed byte that is not negative
} cpu_mnem_code_t;

typedef enum cpu_cond_code
{
    CPU_COND_E,  // equal: the zero flag is set
    CPU_COND_NE, // not equal: the zero flag is cleared
    CPU_COND_L,  // less (signed): the sign flag is not equal to the overflow flag
    CPU_COND_GE, // greater or equal (signed): the sign flag is equal to the overflow flag
    CPU_COND_LE, // less or equal (signed): the zero flag is set or the sign flag is not equal to the overflow flag
    CPU_COND_G,  // greater (signed): the zero flag is cleared and the sign flag is equal to the overflow flag
    CPU_COND_B,  // below (unsigned): the carry flag is set
    CPU_COND_AE, // above or equal (unsigned): the carry flag is cleared
    CPU_COND_BE, // below or equal (unsigned): the carry flag is set or the zero flag is set
    CPU_COND_A   // above (unsigned): the carry flag is cleared and the zero flag is cleared
} cpu_cond_code_t;

typedef enum cpu_reg_code
{
    CPU_REG_R0,
    CPU_REG_R1,
    CPU_REG_R2,
    CPU_REG_R3,
    CPU_REG_R4,
    CPU_REG_R5,
    CPU_REG_R6,
    CPU_REG_R7,
    CPU_REG_FP,
    CPU_REG_SP,
    CPU_REG_LR,
    CPU_REG_IP,
    CPU_REG_FLAGS
} cpu_reg_code_t;

typedef enum cpu_ctrl_reg_code
{
    CPU_CTRL_REG_CTRL,        // `cr0`
    CPU_CTRL_REG_SAVED_IP,    // `cr1`
    CPU_CTRL_REG_FAULT_ADDR,  // `cr2`
    CPU_CTRL_REG_PT_ROOT,     // `cr3`
    CPU_CTRL_REG_SAVED_FLAGS, // `cr4`
    CPU_CTRL_REG_CAUSE,       // `cr5`
    CPU_CTRL_REG_USP,         // `cr6`
    CPU_CTRL_REG_TLB_LOAD,    // `cr7`
    CPU_CTRL_REG_DBG_LOCK     // `cr8`
} cpu_ctrl_reg_code_t;

typedef enum cpu_dot_reg_code
{
    CPU_DOT_REG_W0,     // `w0` to `w3`, the weights of the rows of the array; a row that the array does not have is an invalid register
    CPU_DOT_REG_X0 = 4  // `x0` to `x3`, the inputs of the rows
} cpu_dot_reg_code_t;

/* Operands of an instruction, in the order of the assembly line */
typedef enum cpu_inst_fmt
{
    CPU_FMT_NONE,  // no operands                                (`ret`)
    CPU_FMT_A,     // source register                            (`push r0`)
    CPU_FMT_B,     // destination register                       (`pop r0`)
    CPU_FMT_AB,    // source register, destination register      (`add r0 r1`)
    CPU_FMT_I,     // immediate value                            (`push 0x10`)
    CPU_FMT_IB,    // immediate value, destination register      (`add 0x10 r1`)
    CPU_FMT_REL,   // target relative to the instruction itself  (`jmp label`)
    CPU_FMT_LOAD,  // memory at register A plus the immediate value, destination register  (`lr 0x8(fp) r0`)
    CPU_FMT_STORE, // source register, memory at register B plus the immediate value       (`sr r0 0x8(fp)`)
    CPU_FMT_ABC,   // two source registers, destination register in the low 4 bits of the immediate value  (`dot4 r0 r1 r2`)
    CPU_FMT_CB,    // control register in place of register A, destination register        (`mov cr2 r0`)
    CPU_FMT_AC,    // source register, control register in place of register B             (`mov r0 cr3`)
    CPU_FMT_DB,    // register of the array in place of register A, destination register   (`mov w0 r0`)
    CPU_FMT_AD     // source register, register of the array in place of register B        (`mov r0 w0`)
} cpu_inst_fmt_t;

typedef struct inst_enc
{
    const char *mnem;   // name in assembly
    uint8_t code;       // opcode
    cpu_inst_fmt_t fmt; // operands
    bool privileged;    // raises the privileged instruction exception in user mode
} inst_enc_t;

typedef struct reg_enc
{
    const char *name;
    uint8_t code;
} reg_enc_t;

static const inst_enc_t INST_ENC_TABLE[] = {
    { "hlt",   CPU_MNEM_HLT,                 CPU_FMT_NONE,  true  },
    { "nop",   CPU_MNEM_NOP,                 CPU_FMT_NONE,  false },
    { "cli",   CPU_MNEM_CLI,                 CPU_FMT_NONE,  true  },
    { "sti",   CPU_MNEM_STI,                 CPU_FMT_NONE,  true  },
    { "iret",  CPU_MNEM_IRET,                CPU_FMT_NONE,  true  },
    { "int",   CPU_MNEM_INT  | CPU_OPC_IMM,  CPU_FMT_I,     false },
    { "inb",   CPU_MNEM_INB,                 CPU_FMT_AB,    true  },
    { "outb",  CPU_MNEM_OUTB,                CPU_FMT_AB,    true  },
    { "mov",   CPU_MNEM_MFC,                 CPU_FMT_CB,    true  },
    { "mov",   CPU_MNEM_MTC,                 CPU_FMT_AC,    true  },

    { "mov",   CPU_MNEM_MOV,                 CPU_FMT_AB,    false },
    { "li",    CPU_MNEM_MOV  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "lih",   CPU_MNEM_LIH  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "push",  CPU_MNEM_PUSH,                CPU_FMT_A,     false },
    { "push",  CPU_MNEM_PUSH | CPU_OPC_IMM,  CPU_FMT_I,     false },
    { "pop",   CPU_MNEM_POP,                 CPU_FMT_B,     false },
    { "sr",    CPU_MNEM_SR,                  CPU_FMT_STORE, false },
    { "srh",   CPU_MNEM_SRH,                 CPU_FMT_STORE, false },
    { "srb",   CPU_MNEM_SRB,                 CPU_FMT_STORE, false },
    { "lr",    CPU_MNEM_LR,                  CPU_FMT_LOAD,  false },
    { "lrh",   CPU_MNEM_LRH,                 CPU_FMT_LOAD,  false },
    { "lrb",   CPU_MNEM_LRB,                 CPU_FMT_LOAD,  false },
    { "lrhu",  CPU_MNEM_LRHU,                CPU_FMT_LOAD,  false },
    { "lrbu",  CPU_MNEM_LRBU,                CPU_FMT_LOAD,  false },

    { "add",   CPU_MNEM_ADD,                 CPU_FMT_AB,    false },
    { "add",   CPU_MNEM_ADD  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "adc",   CPU_MNEM_ADC,                 CPU_FMT_AB,    false },
    { "adc",   CPU_MNEM_ADC  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "sub",   CPU_MNEM_SUB,                 CPU_FMT_AB,    false },
    { "sub",   CPU_MNEM_SUB  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "sbb",   CPU_MNEM_SBB,                 CPU_FMT_AB,    false },
    { "sbb",   CPU_MNEM_SBB  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "cmp",   CPU_MNEM_CMP,                 CPU_FMT_AB,    false },
    { "cmp",   CPU_MNEM_CMP  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "and",   CPU_MNEM_AND,                 CPU_FMT_AB,    false },
    { "and",   CPU_MNEM_AND  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "test",  CPU_MNEM_TEST,                CPU_FMT_AB,    false },
    { "test",  CPU_MNEM_TEST | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "or",    CPU_MNEM_OR,                  CPU_FMT_AB,    false },
    { "or",    CPU_MNEM_OR   | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "xor",   CPU_MNEM_XOR,                 CPU_FMT_AB,    false },
    { "xor",   CPU_MNEM_XOR  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "not",   CPU_MNEM_NOT,                 CPU_FMT_B,     false },
    { "shl",   CPU_MNEM_SHL,                 CPU_FMT_AB,    false },
    { "shl",   CPU_MNEM_SHL  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "shr",   CPU_MNEM_SHR,                 CPU_FMT_AB,    false },
    { "shr",   CPU_MNEM_SHR  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "sar",   CPU_MNEM_SAR,                 CPU_FMT_AB,    false },
    { "sar",   CPU_MNEM_SAR  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "mul",   CPU_MNEM_MUL,                 CPU_FMT_AB,    false },
    { "mul",   CPU_MNEM_MUL  | CPU_OPC_IMM,  CPU_FMT_IB,    false },
    { "dot4",  CPU_MNEM_DOT4,                CPU_FMT_ABC,   false },
    { "popcnt", CPU_MNEM_POPCNT,             CPU_FMT_AB,    false },

    { "jmp",   CPU_MNEM_JMP,                 CPU_FMT_A,     false },
    { "jmp",   CPU_MNEM_JMP  | CPU_OPC_IMM,  CPU_FMT_REL,   false },
    { "call",  CPU_MNEM_CALL,                CPU_FMT_A,     false },
    { "call",  CPU_MNEM_CALL | CPU_OPC_IMM,  CPU_FMT_REL,   false },
    { "ret",   CPU_MNEM_RET,                 CPU_FMT_NONE,  false },

    { "je",    CPU_MNEM_JCC | CPU_COND_E,                 CPU_FMT_A,   false },
    { "je",    CPU_MNEM_JCC | CPU_COND_E  | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jne",   CPU_MNEM_JCC | CPU_COND_NE,                CPU_FMT_A,   false },
    { "jne",   CPU_MNEM_JCC | CPU_COND_NE | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jl",    CPU_MNEM_JCC | CPU_COND_L,                 CPU_FMT_A,   false },
    { "jl",    CPU_MNEM_JCC | CPU_COND_L  | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jge",   CPU_MNEM_JCC | CPU_COND_GE,                CPU_FMT_A,   false },
    { "jge",   CPU_MNEM_JCC | CPU_COND_GE | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jle",   CPU_MNEM_JCC | CPU_COND_LE,                CPU_FMT_A,   false },
    { "jle",   CPU_MNEM_JCC | CPU_COND_LE | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jg",    CPU_MNEM_JCC | CPU_COND_G,                 CPU_FMT_A,   false },
    { "jg",    CPU_MNEM_JCC | CPU_COND_G  | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jb",    CPU_MNEM_JCC | CPU_COND_B,                 CPU_FMT_A,   false },
    { "jb",    CPU_MNEM_JCC | CPU_COND_B  | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jae",   CPU_MNEM_JCC | CPU_COND_AE,                CPU_FMT_A,   false },
    { "jae",   CPU_MNEM_JCC | CPU_COND_AE | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "jbe",   CPU_MNEM_JCC | CPU_COND_BE,                CPU_FMT_A,   false },
    { "jbe",   CPU_MNEM_JCC | CPU_COND_BE | CPU_OPC_IMM,  CPU_FMT_REL, false },
    { "ja",    CPU_MNEM_JCC | CPU_COND_A,                 CPU_FMT_A,   false },
    { "ja",    CPU_MNEM_JCC | CPU_COND_A  | CPU_OPC_IMM,  CPU_FMT_REL, false },

    { "sete",  CPU_MNEM_SETCC | CPU_COND_E,  CPU_FMT_B,     false },
    { "setne", CPU_MNEM_SETCC | CPU_COND_NE, CPU_FMT_B,     false },
    { "setl",  CPU_MNEM_SETCC | CPU_COND_L,  CPU_FMT_B,     false },
    { "setge", CPU_MNEM_SETCC | CPU_COND_GE, CPU_FMT_B,     false },
    { "setle", CPU_MNEM_SETCC | CPU_COND_LE, CPU_FMT_B,     false },
    { "setg",  CPU_MNEM_SETCC | CPU_COND_G,  CPU_FMT_B,     false },
    { "setb",  CPU_MNEM_SETCC | CPU_COND_B,  CPU_FMT_B,     false },
    { "setae", CPU_MNEM_SETCC | CPU_COND_AE, CPU_FMT_B,     false },
    { "setbe", CPU_MNEM_SETCC | CPU_COND_BE, CPU_FMT_B,     false },
    { "seta",  CPU_MNEM_SETCC | CPU_COND_A,  CPU_FMT_B,     false },

    { "mov",   CPU_MNEM_MFA,                 CPU_FMT_DB,    false },
    { "mov",   CPU_MNEM_MTA,                 CPU_FMT_AD,    false },
    { "dotv",  CPU_MNEM_DOTV,                CPU_FMT_B,     false },
    { "dotm",  CPU_MNEM_DOTM,                CPU_FMT_B,     false },
    { "learn", CPU_MNEM_LEARN,               CPU_FMT_A,     false },
    { "xsh",   CPU_MNEM_XSH,                 CPU_FMT_A,     false },
    { "ssat",  CPU_MNEM_SSAT,                CPU_FMT_B,     false },
    { "usat",  CPU_MNEM_USAT,                CPU_FMT_B,     false }
};

#define CPU_INST (sizeof(INST_ENC_TABLE) / sizeof(INST_ENC_TABLE[0])) // number of instruction encodings

static const reg_enc_t REG_ENC_TABLE[CPU_REGS] = {
    { "r0",    CPU_REG_R0 },
    { "r1",    CPU_REG_R1 },
    { "r2",    CPU_REG_R2 },
    { "r3",    CPU_REG_R3 },
    { "r4",    CPU_REG_R4 },
    { "r5",    CPU_REG_R5 },
    { "r6",    CPU_REG_R6 },
    { "r7",    CPU_REG_R7 },
    { "fp",    CPU_REG_FP },
    { "sp",    CPU_REG_SP },
    { "lr",    CPU_REG_LR },
    { "ip",    CPU_REG_IP },
    { "flags", CPU_REG_FLAGS }
};

static const reg_enc_t CTRL_REG_ENC_TABLE[CPU_CTRL_REGS] = {
    { "cr0", CPU_CTRL_REG_CTRL },
    { "cr1", CPU_CTRL_REG_SAVED_IP },
    { "cr2", CPU_CTRL_REG_FAULT_ADDR },
    { "cr3", CPU_CTRL_REG_PT_ROOT },
    { "cr4", CPU_CTRL_REG_SAVED_FLAGS },
    { "cr5", CPU_CTRL_REG_CAUSE },
    { "cr6", CPU_CTRL_REG_USP },
    { "cr7", CPU_CTRL_REG_TLB_LOAD },
    { "cr8", CPU_CTRL_REG_DBG_LOCK }
};

static const reg_enc_t DOT_REG_ENC_TABLE[CPU_DOT_REGS] = {
    { "w0", CPU_DOT_REG_W0 },
    { "w1", CPU_DOT_REG_W0 + 1 },
    { "w2", CPU_DOT_REG_W0 + 2 },
    { "w3", CPU_DOT_REG_W0 + 3 },
    { "x0", CPU_DOT_REG_X0 },
    { "x1", CPU_DOT_REG_X0 + 1 },
    { "x2", CPU_DOT_REG_X0 + 2 },
    { "x3", CPU_DOT_REG_X0 + 3 }
};

/* Executable Format */

/*
    A firmware image is a header followed by the text section and the data
    section. The text section becomes the boot ROM, where the CPU starts after
    reset, and the data section is placed in RAM at `FIRMWARE_DATA_ADDR`. The
    rest of RAM keeps its bytes over a reset of the real machine, so a program
    must not count on zeros there.
*/

#define BIN_FMT_MAGIC 0x61736232 /* "asb2" */

typedef struct __attribute__((packed)) bin_fmt_header
{
    uint32_t magic;
    uint32_t data_sec_size;
    uint32_t text_sec_size;
} bin_fmt_header_t;

typedef struct firmware
{
    uint8_t *image;
    size_t image_size;

    uint8_t *data_sec;
    size_t data_sec_size;

    uint8_t *text_sec;
    size_t text_sec_size;
} firmware_t;

/* Object File Format */

/*
    The assembler and the C compiler write ELF32 relocatable objects for the
    linker. A relocation is the request to place the address S of a symbol,
    plus a constant A, into a field at the address P once the linker knows
    where everything goes. The relocation sections are of the kind that keeps
    the constant A in the relocation entry (`.rela`).
*/

#define ELF_MACHINE_ASC2 0xA5C2 // machine number of Almurut II in the ELF header (private, not registered)

#define ELF_RELOC_ASC2_NONE  0  // no relocation
#define ELF_RELOC_ASC2_32    1  // 32-bit word in data: S + A
#define ELF_RELOC_ASC2_LO16  2  // immediate value of the instruction at P (`li`): the lower 16 bits of S + A
#define ELF_RELOC_ASC2_HI16  3  // immediate value of the instruction at P (`lih`): the upper 16 bits of S + A
#define ELF_RELOC_ASC2_REL16 4  // immediate value of the jump or `call` at P: (S + A - P) / 4, which must fit in 16 bits with a sign

/*
    The parts of an ELF32 file that the tools read and write. An object file
    describes its contents with section headers. A linked program also has
    program headers: each one names a part of the file and the address where
    that part must be in memory before the program starts.
*/

#define ELF_CLASS_32      1
#define ELF_DATA_LSB      1
#define ELF_VERSION       1
#define ELF_TYPE_REL      1 // an object file for the linker
#define ELF_TYPE_EXEC     2 // a linked program

#define ELF_SEC_PROGBITS  1
#define ELF_SEC_SYMTAB    2
#define ELF_SEC_STRTAB    3
#define ELF_SEC_RELA      4

#define ELF_SEC_FLAG_WRITE 0x1
#define ELF_SEC_FLAG_ALLOC 0x2
#define ELF_SEC_FLAG_EXEC  0x4
#define ELF_SEC_FLAG_INFO  0x40

#define ELF_SYM_LOCAL     0
#define ELF_SYM_GLOBAL    1
#define ELF_SYM_NOTYPE    0

typedef struct elf_header
{
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_t;

typedef struct elf_sec_header
{
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} elf_sec_header_t;

typedef struct elf_sym
{
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
} elf_sym_t;

typedef struct elf_rela
{
    uint32_t offset;
    uint32_t info;
    int32_t  addend;
} elf_rela_t;

#define ELF_PROG_LOAD     1 // a program header of a part to load into memory

typedef struct elf_prog_header
{
    uint32_t type;
    uint32_t offset; // position of the part in the file
    uint32_t vaddr;  // address of the part in memory
    uint32_t paddr;
    uint32_t filesz; // bytes of the part in the file
    uint32_t memsz;  // bytes of the part in memory; those beyond `filesz` start as zeros
    uint32_t flags;
    uint32_t align;
} elf_prog_header_t;

/* Helpful Constants */

/* CPU's Flag Bits */

#define CPU_FLAG_C 0b00000001
#define CPU_FLAG_Z 0b00000010
#define CPU_FLAG_S 0b00000100
#define CPU_FLAG_O 0b00001000
#define CPU_FLAG_I 0b00010000
#define CPU_FLAG_U 0b00100000

#define CPU_FLAGS_ARITH (CPU_FLAG_C | CPU_FLAG_Z | CPU_FLAG_S | CPU_FLAG_O) // flags that an ordinary write to `flags` can change
#define CPU_FLAGS_ALL   (CPU_FLAGS_ARITH | CPU_FLAG_I | CPU_FLAG_U)

/* CPU's Control Register Bits */

#define CPU_CTRL_P 0b00000001
#define CPU_CTRL_S 0b00000010
#define CPU_CTRL_F 0b00000100
#define CPU_CTRL_T 0b00001000
#define CPU_CTRL_C 0b00010000
#define CPU_CTRL_A 0b00100000

#define CPU_CTRL_ALL (CPU_CTRL_P | CPU_CTRL_S | CPU_CTRL_F | CPU_CTRL_T | CPU_CTRL_C | CPU_CTRL_A)

#define CPU_DBG_LOCK_KEY 0x4c4f434b // value that locks the debug port when it is written to `cr8`: the letters "LOCK"

/* CPU's Cause Register Bits */

#define CPU_CAUSE_VEC_MASK   0x000000ff
#define CPU_CAUSE_INT_SHIFT  8
#define CPU_CAUSE_P          0x00010000
#define CPU_CAUSE_W          0x00020000
#define CPU_CAUSE_U          0x00040000
#define CPU_CAUSE_F          0x00080000
#define CPU_CAUSE_LVL_SHIFT  20
#define CPU_CAUSE_M          0x00400000
#define CPU_CAUSE_STAT_SHIFT 24
#define CPU_CAUSE_STAT_MASK  0x07000000

#define CPU_CAUSE_ALL        0x077fffff

/* Keyboard Controller's Flag Bits */

#define KBDC_FLAG_WAITING 0b00000001
#define KBDC_FLAG_OVERRUN 0b00000010

/* RP2040's Register Bits */

#define RP_ID              0x324d4c41 /* "ALM2" */
#define RP_CON_FLAG_INPUT  0b00000001
#define RP_CON_FLAG_OUTPUT 0b00000010
#define RP_CON_NO_DATA     0x100
#define RP_IRQ_FLAG_CON    0b00000001
#define RP_IRQ_FLAG_DISK   0b00000010
#define RP_IRQ_FLAG_BTN    0b00000100
#define RP_IRQ_FLAGS       (RP_IRQ_FLAG_CON | RP_IRQ_FLAG_DISK | RP_IRQ_FLAG_BTN)
#define RP_BTN_FLAG_DOWN   0b00000001
#define RP_SCR_FLAG_BUSY   0b00000001
#define RP_SCR_CMD_SHOW    1
#define RP_SCR_CMD_CLEAR   2
#define RP_SCR_CMD_CONSOLE 3
#define RP_SCR_DARK        0    // ways to draw: the pixels go dark,
#define RP_SCR_LIGHT       1    // they light up,
#define RP_SCR_FLIP        2    // each becomes its opposite,
#define RP_SCR_COPY        3    // or (a picture only) they become the pixels of the picture, lit and dark
#define RP_FONT_FIRST      0x20 // first and last character of the font; any other is drawn as a question mark
#define RP_FONT_LAST       0x7e

// The characters of the screen: five columns each, the top pixel of a column in its bit 0
static const uint8_t RP_FONT[RP_FONT_LAST - RP_FONT_FIRST + 1][RP_SCR_CHAR_WIDTH - 1] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7f, 0x14, 0x7f, 0x14}, {0x24, 0x2a, 0x7f, 0x2a, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62}, {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1c, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1c, 0x00}, {0x14, 0x08, 0x3e, 0x08, 0x14}, {0x08, 0x08, 0x3e, 0x08, 0x08}, {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14}, {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01}, {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7f, 0x41, 0x41, 0x00}, {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7f, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78}, {0x7f, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7f}, {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7e, 0x09, 0x01, 0x02}, {0x0c, 0x52, 0x52, 0x52, 0x3e},
    {0x7f, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7d, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3d, 0x00}, {0x7f, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7f, 0x40, 0x00}, {0x7c, 0x04, 0x18, 0x04, 0x78}, {0x7c, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7c, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7c}, {0x7c, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20}, {0x04, 0x3f, 0x44, 0x40, 0x20}, {0x3c, 0x40, 0x40, 0x20, 0x7c}, {0x1c, 0x20, 0x40, 0x20, 0x1c}, {0x3c, 0x40, 0x30, 0x40, 0x3c},
    {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0c, 0x50, 0x50, 0x50, 0x3c}, {0x44, 0x64, 0x54, 0x4c, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7f, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x04, 0x08, 0x10, 0x08},
};
#define RP_DISK_CMD_READ   1
#define RP_DISK_CMD_WRITE  2
#define RP_DISK_FLAG_BUSY  0b00000001
#define RP_DISK_FLAG_ERROR 0b00000010

/* Answers of the Bus to an Access */

#define BUS_STAT_OK         0 // the access was done
#define BUS_STAT_ERROR      1 // nothing answers at the address, or the device there refuses this kind of access
#define BUS_STAT_PROTECTION 2 // the address is for kernel mode only, or it holds no instructions and the access is a fetch
#define BUS_STAT_MISALIGNED 3 // the address is not a multiple of the access size (the CPU checks this itself and never sends such an access)
#define BUS_STAT_BAD_NEXT   4 // the RP2040 was asked for "the next word" with no earlier fetch to continue (a CPU that follows the rule of sequential fetch never asks so)
#define BUS_STAT_LINK_ERROR 5 // the chip got no valid answer after all its retries; the RP2040 may or may not have done the access (in the emulator only on the order of `RP_LOSE_ADDR`)

/* Initialization Functions */

static void cpu_init(cpu_t *cpu)
{
    assert(cpu != NULL);

    cpu->r0 = 0;
    cpu->r1 = 0;
    cpu->r2 = 0;
    cpu->r3 = 0;
    cpu->r4 = 0;
    cpu->r5 = 0;
    cpu->r6 = 0;
    cpu->r7 = 0;

    cpu->fp  = 0;
    cpu->ksp = 0; // the boot code sets the stack pointer before it uses the stack
    cpu->usp = 0;
    cpu->lr  = 0;
    cpu->ip  = CPU_RESET_IP;
    cpu->flags = 0b00000010; // only Z (Zero flag) is set: kernel mode, interrupts disabled

    cpu->ctrl = CPU_CTRL_F; // paging is off, the chip's own page table lookup is on, every fetch carries its address
    cpu->saved_ip = 0;
    cpu->fault_addr = 0;
    cpu->pt_root = 0;
    cpu->saved_flags = 0;
    cpu->cause = 0;
    cpu->dbg_locked = false;

    memset(cpu->dot_w, 0, sizeof(cpu->dot_w)); // the chip leaves these as they are
    memset(cpu->dot_x, 0, sizeof(cpu->dot_x));

    for (size_t i = 0; i < CPU_TLB_SIZE; ++i) {
        cpu->tlb[i].valid = false;
        cpu->tlb[i].vpn = 0;
        cpu->tlb[i].pfn = 0;
        cpu->tlb[i].writable = false;
        cpu->tlb[i].user = false;
    }
    cpu->tlb_next = 0;

    cpu->halted = false;
    cpu->fetch_run = false;
    cpu->double_fault = false;
    memset(cpu->ic_valid, 0, sizeof(cpu->ic_valid));
    memset(cpu->ic_tag, 0, sizeof(cpu->ic_tag));
    memset(cpu->ic_data, 0, sizeof(cpu->ic_data));

    cpu->cycles = 0;
    cpu->next_ip = cpu->ip;
    cpu->trapped = false;
    cpu->retired = false;
    cpu->inst = 0;
}

static void mem_init(mem_t *mem)
{
    assert(mem != NULL);

    /* An empty memory is full of `hlt` instructions, as the code of `hlt` is 0x00. */

    memset(mem->ram, 0, RAM_SIZE);
    memset(mem->rom, 0, ROM_SIZE);
    memset(mem->high, 0, HIGH_SIZE);
}

static void pic_init(pic_t *pic)
{
    assert(pic != NULL);

    pic->imr = 0b11111111;  // all interrupts are masked at boot
    pic->irr = 0b00000000;  // no requests are pending at boot
    pic->vec_off = CPU_EXCPTS; // the first 8 vectors are reserved for CPU exceptions
    pic->state = PIC_AWAITS_IMR;
}

static void pit_init(pit_t *pit)
{
    assert(pit != NULL);

    pit->freq_div_high = 0; // 0 is interpreted as 65536 in PIT
    pit->freq_div_low  = 0;
    pit->cnt_high = pit->freq_div_high;
    pit->cnt_low  = pit->freq_div_low;
    pit->state = PIT_AWAITS_FREQ_DIV_HIGH;
}

static void kbdc_init(kbdc_t *kbdc)
{
    assert(kbdc != NULL);

    kbdc->scancode = 0;
    kbdc->status = 0b00000000; // no byte is waiting at boot
}

static void rp_init(rp_t *rp)
{
    assert(rp != NULL);

    rp->scratch = 0;
    rp->time_us = 0;

    rp->con_in_pos = 0;
    rp->con_in_cnt = 0;

    rp->irq_stat = 0;
    rp->irq_enable = 0; // no device of the RP2040 raises the interrupt line at boot

    rp->disk_sec = 0;
    rp->disk_buf = 0;
    rp->disk_cmd = 0;
    rp->disk_stat = 0;

    rp->fetch_addr = 0;
    rp->fetch_valid = false;
    rp->lose_cnt = 0;

    rp->btn_down = false;
    rp->btn_count = 0;

    memset(rp->scr, 0, RP_SCR_SIZE); // the frame memory is dark at boot
    rp->scr_point = 0;
    rp->scr_src = 0;
    memset(rp->scr_pic, 0, sizeof(rp->scr_pic));
    rp->scr_console = true; // the console draws until a program does
    rp->scr_dirty = false;
    rp->con_row = 0;
    rp->con_col = 0;
    rp->con_esc = 0;
    rp->scr_stat = 0;
    rp->scr_frames = 0;
    memset(rp->scr_sent, 0, RP_SCR_SIZE);
    memset(rp->scr_shown, 0, RP_SCR_SIZE);
    rp->scr_steps = 0;

    rp->con_out = 0;
    rp->con_out_valid = false;
    rp->disk = NULL; // no disk until `rp_attach_disk`
    rp->disk_size = 0;
    rp->disk_steps = 0;
}

static void dev_init(device_t *dev, firmware_t *firmware)
{
    assert(dev != NULL);

    assert(firmware != NULL);
    assert(firmware->image != NULL);
    assert(firmware->data_sec != NULL);
    assert(firmware->text_sec != NULL);

    assert(firmware->text_sec_size <= MAX_FIRMWARE_TEXT_SIZE);
    assert(firmware->data_sec_size <= MAX_FIRMWARE_DATA_SIZE);
    assert(firmware->image_size == firmware->data_sec_size + firmware->text_sec_size);

    cpu_init(&dev->cpu);
    mem_init(&dev->mem);
    pic_init(&dev->pic);
    pit_init(&dev->pit);
    kbdc_init(&dev->kbdc);
    rp_init(&dev->rp);

    dev->bus_reqs_cnt = 0;

    memcpy(dev->mem.rom + (FIRMWARE_TEXT_ADDR - KERN_BASE - ROM_BASE), firmware->text_sec, firmware->text_sec_size);
    memcpy(dev->mem.ram + (FIRMWARE_DATA_ADDR - KERN_BASE - RAM_BASE), firmware->data_sec, firmware->data_sec_size);
}

/* The RP2040's Side of the Bus */

static void rp_attach_disk(device_t *dev, uint8_t *disk, size_t disk_size)
{
    assert(dev != NULL);

    dev->rp.disk = disk;
    dev->rp.disk_size = disk_size;
}

static bool rp_put_con_input(device_t *dev, uint8_t byte)
{
    assert(dev != NULL);

    rp_t *rp = &dev->rp;

    if (rp->con_in_cnt == RP_CON_BUFF_SIZE) { // the queue is full; the sender has to wait and try again
        return false;
    }

    rp->con_in[(rp->con_in_pos + rp->con_in_cnt) % RP_CON_BUFF_SIZE] = byte;
    ++rp->con_in_cnt;

    return true;
}

static bool rp_take_con_output(device_t *dev, uint8_t *byte)
{
    assert(dev != NULL);
    assert(byte != NULL);

    if (!dev->rp.con_out_valid) {
        return false;
    }

    *byte = dev->rp.con_out;
    dev->rp.con_out_valid = false;

    return true;
}

// Presses or releases the button. A press is counted and requests an interrupt.
static void rp_set_button(device_t *dev, bool is_down)
{
    assert(dev != NULL);

    rp_t *rp = &dev->rp;
    if (is_down && !rp->btn_down) {
        ++rp->btn_count;
        rp->irq_stat |= RP_IRQ_FLAG_BTN;
    }
    rp->btn_down = is_down;
}

// Draws a character into the frame memory at a column and a row of text.
// A place outside the screen draws nothing.
static void rp_draw_char(uint8_t *scr, uint32_t value)
{
    assert(scr != NULL);

    uint32_t code = value & 0xff;
    uint32_t col  = (value >> 8) & 0xff;
    uint32_t row  = (value >> 16) & 0xff;
    if (col >= RP_SCR_COLS || row >= RP_SCR_ROWS) {
        return;
    }
    if (code < RP_FONT_FIRST || code > RP_FONT_LAST) {
        code = '?';
    }

    for (uint32_t i = 0; i < RP_SCR_CHAR_WIDTH; ++i) {
        uint32_t bits = i < RP_SCR_CHAR_WIDTH - 1 ? RP_FONT[code - RP_FONT_FIRST][i] : 0;
        uint32_t x = col * RP_SCR_CHAR_WIDTH + i;
        for (uint32_t j = 0; j < RP_SCR_CHAR_HEIGHT; ++j) {
            uint8_t *byte = scr + (row * RP_SCR_CHAR_HEIGHT + j) * (RP_SCR_WIDTH / 8) + x / 8;
            uint8_t mask = (uint8_t) (1u << (x % 8));
            if ((bits >> j) & 1) {
                *byte |= mask;
            } else {
                *byte &= (uint8_t) ~mask;
            }
        }
    }
}

// Changes one pixel of the frame memory in one of the ways to draw.
static void rp_draw_pixel(uint8_t *scr, uint32_t x, uint32_t y, uint32_t mode)
{
    assert(scr != NULL);
    assert(x < RP_SCR_WIDTH && y < RP_SCR_HEIGHT);

    uint8_t *byte = scr + y * (RP_SCR_WIDTH / 8) + x / 8;
    uint8_t mask = (uint8_t) (1u << (x % 8));
    if (mode == RP_SCR_DARK) {
        *byte &= (uint8_t) ~mask;
    } else if (mode == RP_SCR_LIGHT) {
        *byte |= mask;
    } else {
        *byte ^= mask;
    }
}

// Draws a rectangle into the frame memory. The way "copy" has no meaning
// for a rectangle and draws nothing.
static void rp_draw_rect(uint8_t *scr, uint32_t value)
{
    assert(scr != NULL);

    uint32_t left   = value & 0x7f;
    uint32_t top    = (value >> 7) & 0x3f;
    uint32_t width  = ((value >> 13) & 0xff) + 1;
    uint32_t height = ((value >> 21) & 0x3f) + 1;
    uint32_t mode   = (value >> 27) & 0x3;
    if (mode == RP_SCR_COPY) {
        return;
    }

    for (uint32_t y = top; y < top + height && y < RP_SCR_HEIGHT; ++y) {
        for (uint32_t x = left; x < left + width && x < RP_SCR_WIDTH; ++x) {
            rp_draw_pixel(scr, x, y, mode);
        }
    }
}

/*
    Draws a lit line into the frame memory, from the point `from` to the point
    `to`, with both ends. The algorithm is Bresenham's: the pixel that is
    nearer to the line at every step. A point lies within RP_SCR_POINT_MIN and
    RP_SCR_POINT_MAX; a line with an end outside of that draws nothing, so the
    time of a line has an end. Pixels outside the screen are left out.
*/
static void rp_draw_line(uint8_t *scr, uint32_t from, uint32_t to)
{
    assert(scr != NULL);

    int32_t x0 = (int16_t) (from & 0xffff);
    int32_t y0 = (int16_t) (from >> 16);
    int32_t x1 = (int16_t) (to & 0xffff);
    int32_t y1 = (int16_t) (to >> 16);
    if (x0 < RP_SCR_POINT_MIN || x0 > RP_SCR_POINT_MAX || y0 < RP_SCR_POINT_MIN || y0 > RP_SCR_POINT_MAX ||
        x1 < RP_SCR_POINT_MIN || x1 > RP_SCR_POINT_MAX || y1 < RP_SCR_POINT_MIN || y1 > RP_SCR_POINT_MAX) {
        return;
    }

    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int32_t step_x = x1 > x0 ? 1 : -1;
    int32_t step_y = y1 > y0 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < RP_SCR_WIDTH && y0 >= 0 && y0 < RP_SCR_HEIGHT) {
            rp_draw_pixel(scr, (uint32_t) x0, (uint32_t) y0, RP_SCR_LIGHT);
        }
        if (x0 == x1 && y0 == y1) {
            return;
        }
        int32_t twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += step_x;
        }
        if (twice <= dx) {
            error += dx;
            y0 += step_y;
        }
    }
}

// Returns the byte `off` of a picture that starts at the physical address
// `src` and goes on, after the end of its first page, at `next` (0 if the
// pages lie one after the other).
static uint8_t rp_get_picture_byte(device_t *dev, uint32_t src, uint32_t next, uint32_t off)
{
    uint32_t in_page = (src % PAGE_SIZE) + off;
    uint32_t addr = (next != 0 && in_page >= PAGE_SIZE) ? next + (in_page - PAGE_SIZE) : src + off;

    return dev->mem.ram[addr - RAM_BASE];
}

// Draws the picture at the physical address `src` into the frame memory;
// `next` is the address of the page where the picture goes on, or 0. A
// picture that is not whole in RAM, whose address is not a multiple of 4,
// or that is empty or larger than the screen draws nothing.
static void rp_draw_picture(device_t *dev, uint32_t src, uint32_t next, uint32_t value)
{
    assert(dev != NULL);

    uint8_t *scr = dev->rp.scr;
    int32_t left  = (int8_t) (value & 0xff);
    int32_t top   = (int8_t) ((value >> 8) & 0xff);
    uint32_t mode = (value >> 16) & 0x3;

    if (src % 4 != 0 || src < RAM_BASE || (uint64_t) src + 4 > RAM_BASE + RAM_SIZE) {
        return;
    }
    if (next != 0 && (next % PAGE_SIZE != 0 || next < RAM_BASE || (uint64_t) next + PAGE_SIZE > RAM_BASE + RAM_SIZE)) {
        return;
    }
    uint32_t width  = (uint32_t) rp_get_picture_byte(dev, src, next, 0) | (uint32_t) rp_get_picture_byte(dev, src, next, 1) << 8;
    uint32_t height = (uint32_t) rp_get_picture_byte(dev, src, next, 2) | (uint32_t) rp_get_picture_byte(dev, src, next, 3) << 8;
    uint32_t row_size = (width + 31) / 32 * 4;
    if (width == 0 || width > RP_SCR_WIDTH || height == 0 || height > RP_SCR_HEIGHT ||
        (next == 0 && (uint64_t) src + 4 + (uint64_t) row_size * height > RAM_BASE + RAM_SIZE)) {
        return;
    }

    for (uint32_t j = 0; j < height; ++j) {
        int32_t y = top + (int32_t) j;
        if (y < 0 || y >= RP_SCR_HEIGHT) {
            continue;
        }
        for (uint32_t i = 0; i < width; ++i) {
            int32_t x = left + (int32_t) i;
            if (x < 0 || x >= RP_SCR_WIDTH) {
                continue;
            }
            bool is_lit = (rp_get_picture_byte(dev, src, next, 4 + j * row_size + i / 8) >> (i % 8)) & 1;
            if (mode == RP_SCR_COPY) {
                rp_draw_pixel(scr, (uint32_t) x, (uint32_t) y, is_lit ? RP_SCR_LIGHT : RP_SCR_DARK);
            } else if (is_lit) {
                rp_draw_pixel(scr, (uint32_t) x, (uint32_t) y, mode);
            }
        }
    }
}

// Draws or wipes out the cursor of the console.
static void rp_draw_cursor(rp_t *rp, bool is_shown)
{
    if (rp->con_col < RP_SCR_COLS) {
        rp_draw_char(rp->scr, rp->con_row << 16 | rp->con_col << 8 | (is_shown ? '_' : ' '));
    }
}

// Starts the next row of the console; the rows move up when the last one is full.
static void rp_start_console_row(rp_t *rp)
{
    uint32_t row_size = RP_SCR_CHAR_HEIGHT * (RP_SCR_WIDTH / 8);

    rp->con_col = 0;
    if (++rp->con_row == RP_SCR_ROWS) {
        memmove(rp->scr, rp->scr + row_size, RP_SCR_SIZE - row_size);
        memset(rp->scr + RP_SCR_SIZE - row_size, 0, row_size);
        rp->con_row = RP_SCR_ROWS - 1;
    }
}

/*
    Draws a byte that the CPU has printed, as a terminal does: a new line, a
    return to the start of the row, a step back, and the characters of the
    font. A row that is full goes on in the next one. An escape sequence of
    a terminal (the escape byte, a bracket, and bytes up to a letter) is
    left out, and so is any other byte.
*/
static void rp_put_console_byte(rp_t *rp, uint8_t byte)
{
    assert(rp != NULL);

    rp_draw_cursor(rp, false);

    if (rp->con_esc == 1) {
        rp->con_esc = byte == '[' ? 2 : 0;
    } else if (rp->con_esc == 2) {
        if (byte >= 0x40 && byte <= 0x7e) {
            rp->con_esc = 0;
        }
    } else if (byte == 0x1b) {
        rp->con_esc = 1;
    } else if (byte == '\n') {
        rp_start_console_row(rp);
    } else if (byte == '\r') {
        rp->con_col = 0;
    } else if (byte == '\b' || byte == 0x7f) {
        if (rp->con_col > 0) {
            --rp->con_col;
        }
    } else if (byte >= RP_FONT_FIRST && byte <= RP_FONT_LAST) {
        if (rp->con_col == RP_SCR_COLS) {
            rp_start_console_row(rp);
        }
        rp_draw_char(rp->scr, rp->con_row << 16 | rp->con_col << 8 | byte);
        ++rp->con_col;
    }

    rp_draw_cursor(rp, true);
    rp->scr_dirty = true;
}

// Takes the screen from the console for a program that draws: the frame memory starts dark.
static void rp_leave_console(rp_t *rp)
{
    if (rp->scr_console) {
        rp->scr_console = false;
        memset(rp->scr, 0, RP_SCR_SIZE);
    }
}

static uint32_t rp_get_irq_stat(rp_t *rp)
{
    assert(rp != NULL);

    uint32_t stat = rp->irq_stat;
    if (rp->con_in_cnt > 0) {
        stat |= RP_IRQ_FLAG_CON;
    }

    return stat;
}

static bool rp_get_irq_line(rp_t *rp)
{
    assert(rp != NULL);

    return (rp_get_irq_stat(rp) & rp->irq_enable) != 0;
}

static uint8_t rp_access_reg(device_t *dev, uint32_t addr, bool write, uint32_t *data)
{
    assert(dev != NULL);
    assert(data != NULL);

    rp_t *rp = &dev->rp;

    // A running command keeps its sector, its buffer, and itself: writes to the disk's registers are ignored until it ends
    bool is_disk_busy = (rp->disk_stat & RP_DISK_FLAG_BUSY) != 0;

    // A register of the user page works as its twin of the kernel's page, with two differences below
    bool is_user_reg = addr >= RP_USER_SCR_STAT_ADDR && addr <= RP_USER_SCR_LINE_ADDR;
    if (is_user_reg) {
        static const uint32_t TWINS[] = {
            RP_SCR_STAT_ADDR, RP_SCR_CMD_ADDR, RP_SCR_FRAMES_ADDR, RP_SCR_CHAR_ADDR, RP_SCR_RECT_ADDR,
            RP_SCR_BLIT_ADDR, RP_BTN_STAT_ADDR, RP_BTN_COUNT_ADDR, RP_TIME_ADDR, RP_SCR_MOVE_ADDR, RP_SCR_LINE_ADDR
        };
        addr = TWINS[(addr - RP_USER_SCR_STAT_ADDR) / 4];
    }

    if (addr == RP_ID_ADDR && !write) {
        *data = RP_ID;
    } else if (addr == RP_SCRATCH_ADDR) {
        if (write) {
            rp->scratch = *data;
        } else {
            *data = rp->scratch;
        }
    } else if (addr == RP_RAM_SIZE_ADDR && !write) {
        *data = RAM_SIZE;
    } else if (addr == RP_TIME_ADDR && !write) {
        *data = rp->time_us;
    } else if (addr == RP_CON_STAT_ADDR && !write) {
        *data = RP_CON_FLAG_OUTPUT; // the emulated console can always take a byte
        if (rp->con_in_cnt > 0) {
            *data |= RP_CON_FLAG_INPUT;
        }
    } else if (addr == RP_CON_DATA_ADDR) {
        if (write) {
            rp->con_out = (uint8_t) *data;
            rp->con_out_valid = true;
            if (rp->scr_console) {
                rp_put_console_byte(rp, (uint8_t) *data);
            }
        } else if (rp->con_in_cnt > 0) {
            *data = rp->con_in[rp->con_in_pos];
            rp->con_in_pos = (rp->con_in_pos + 1) % RP_CON_BUFF_SIZE;
            --rp->con_in_cnt;
        } else {
            *data = RP_CON_NO_DATA;
        }
    } else if (addr == RP_IRQ_STAT_ADDR) {
        if (write) {
            rp->irq_stat &= ~(*data & (RP_IRQ_FLAG_DISK | RP_IRQ_FLAG_BTN));
        } else {
            *data = rp_get_irq_stat(rp);
        }
    } else if (addr == RP_IRQ_ENABLE_ADDR) {
        if (write) {
            rp->irq_enable = *data & RP_IRQ_FLAGS;
        } else {
            *data = rp->irq_enable;
        }
    } else if (addr == RP_DISK_SEC_ADDR) {
        if (!write) {
            *data = rp->disk_sec;
        } else if (!is_disk_busy) {
            rp->disk_sec = *data;
        }
    } else if (addr == RP_DISK_BUF_ADDR) {
        if (!write) {
            *data = rp->disk_buf;
        } else if (!is_disk_busy) {
            rp->disk_buf = *data;
        }
    } else if (addr == RP_DISK_CMD_ADDR) {
        if (!write) {
            *data = rp->disk_cmd;
        } else if (!is_disk_busy) {
            rp->disk_cmd = *data;
            rp->disk_stat = RP_DISK_FLAG_BUSY; // the command ends in `rp_step`
            rp->disk_steps = RP_DISK_STEPS;
        }
    } else if (addr == RP_DISK_STAT_ADDR && !write) {
        *data = rp->disk_stat;
    } else if (addr == RP_LOSE_ADDR) {
        if (write) {
            rp->lose_cnt = *data;
        } else {
            *data = rp->lose_cnt;
        }
    } else if (addr == RP_BTN_STAT_ADDR && !write) {
        *data = rp->btn_down ? RP_BTN_FLAG_DOWN : 0;
    } else if (addr == RP_BTN_COUNT_ADDR && !write) {
        *data = rp->btn_count;
    } else if (addr == RP_SCR_STAT_ADDR && !write) {
        *data = rp->scr_stat;
    } else if (addr == RP_SCR_CMD_ADDR) {
        bool is_scr_busy = (rp->scr_stat & RP_SCR_FLAG_BUSY) != 0;
        if (!write) {
            *data = is_scr_busy ? RP_SCR_CMD_SHOW : 0;
        } else if (*data == RP_SCR_CMD_CLEAR) {
            rp_leave_console(rp);
            memset(rp->scr, 0, RP_SCR_SIZE);
        } else if (*data == RP_SCR_CMD_SHOW) {
            rp_leave_console(rp);
            if (!is_scr_busy) {
                memcpy(rp->scr_sent, rp->scr, RP_SCR_SIZE); // the frame arrives in `rp_step`
                rp->scr_stat = RP_SCR_FLAG_BUSY;
                rp->scr_steps = RP_SCR_STEPS;
            }
        } else if (*data == RP_SCR_CMD_CONSOLE && !is_user_reg) {
            // Only a kernel gives the screen back to the console
            rp->scr_console = true;
            memset(rp->scr, 0, RP_SCR_SIZE);
            rp->con_row = 0;
            rp->con_col = 0;
            rp->con_esc = 0;
            rp_draw_cursor(rp, true);
            rp->scr_dirty = true;
        }
    } else if (addr == RP_SCR_FRAMES_ADDR && !write) {
        *data = rp->scr_frames;
    } else if (addr == RP_SCR_CHAR_ADDR && write) {
        rp_leave_console(rp);
        rp_draw_char(rp->scr, *data);
    } else if (addr == RP_SCR_RECT_ADDR && write) {
        rp_leave_console(rp);
        rp_draw_rect(rp->scr, *data);
    } else if (addr == RP_SCR_MOVE_ADDR && write) {
        rp->scr_point = *data;
    } else if (addr == RP_SCR_LINE_ADDR && write) {
        rp_leave_console(rp);
        rp_draw_line(rp->scr, rp->scr_point, *data);
        rp->scr_point = *data;
    } else if (addr >= RP_SCR_PIC_ADDR && addr < RP_SCR_PIC_ADDR + RP_SCR_PICS * 8) {
        uint32_t *reg = &rp->scr_pic[(addr - RP_SCR_PIC_ADDR) / 8][(addr - RP_SCR_PIC_ADDR) / 4 % 2];
        if (write) {
            *reg = *data;
        } else {
            *data = *reg;
        }
    } else if (addr == RP_SCR_SRC_ADDR) {
        if (write) {
            rp->scr_src = *data;
        } else {
            *data = rp->scr_src;
        }
    } else if (addr == RP_SCR_BLIT_ADDR && write) {
        rp_leave_console(rp);
        if (is_user_reg) {
            // A user program draws one of the pictures that the kernel has named
            uint32_t *pic = rp->scr_pic[(*data >> 18) & (RP_SCR_PICS - 1)];
            // A picture that the kernel has not named, with both of its addresses, draws nothing
            if (pic[0] != 0 && pic[1] != 0) {
                rp_draw_picture(dev, pic[0], pic[1], *data);
            }
        } else {
            rp_draw_picture(dev, rp->scr_src, 0, *data);
        }
    } else if (addr >= RP_SCR_BASE && addr < RP_SCR_BASE + RP_SCR_SIZE) {
        uint8_t *bytes = rp->scr + (addr - RP_SCR_BASE);
        if (write) {
            rp_leave_console(rp);
            for (uint8_t i = 0; i < 4; ++i) {
                bytes[i] = (uint8_t) (*data >> (8 * i));
            }
        } else {
            *data = 0;
            for (uint8_t i = 0; i < 4; ++i) {
                *data |= (uint32_t) bytes[i] << (8 * i);
            }
        }
    } else {
        return BUS_STAT_ERROR; // no such register, or a write to a read-only one
    }

    return BUS_STAT_OK;
}

/*
    Performs one access as the RP2040 does it. `addr` is a physical address,
    and the CPU has already checked that it is a multiple of `size`. Values
    are stored with the low byte first.
*/
static uint8_t rp_access(device_t *dev, uint32_t addr, uint8_t size, mem_access_t access, bool kernel, uint32_t *data)
{
    assert(dev != NULL);
    assert(data != NULL);
    assert(size == 1 || size == 2 || size == 4);
    assert(addr % size == 0);

    uint8_t *bytes = NULL;
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        bytes = dev->mem.ram + (addr - RAM_BASE);
    } else if (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE) {
        if (access == MEM_WRITE) {
            return BUS_STAT_ERROR;
        }
        bytes = dev->mem.rom + (addr - ROM_BASE);
    } else if (addr >= HIGH_BASE && addr - HIGH_BASE < HIGH_SIZE) {
        bytes = dev->mem.high + (addr - HIGH_BASE);
    } else if (addr >= RP_BASE && addr < RP_BASE + RP_SIZE) {
        // A user program reaches the page of the screen and the button, and no other
        bool is_user_page = addr >= RP_USER_BASE && addr < RP_USER_BASE + RP_USER_SIZE;
        if ((!kernel && !is_user_page) || access == MEM_FETCH) {
            return BUS_STAT_PROTECTION;
        }
        if (size != 4) {
            return BUS_STAT_ERROR;
        }
        return rp_access_reg(dev, addr, access == MEM_WRITE, data);
    } else {
        return BUS_STAT_ERROR;
    }

    if (access == MEM_WRITE) {
        for (uint8_t i = 0; i < size; ++i) {
            bytes[i] = (uint8_t) (*data >> (8 * i));
        }
    } else {
        *data = 0;
        for (uint8_t i = 0; i < size; ++i) {
            *data |= (uint32_t) bytes[i] << (8 * i);
        }
    }

    return BUS_STAT_OK;
}

/*
    Sends one request of the CPU over the bus and returns the answer that the
    chip gets. With `next` set the request is a fetch of "the next word": it
    carries no address, and the RP2040 reads where its last fetch has ended.
*/
static uint8_t bus_access(device_t *dev, uint32_t addr, uint8_t size, mem_access_t access, bool kernel, bool next, uint32_t *data)
{
    assert(dev != NULL);
    assert(data != NULL);
    assert(!next || access == MEM_FETCH);

    rp_t *rp = &dev->rp;

    // The test register counts every access after the write that has set it
    bool is_lost = rp->lose_cnt > 0 && --rp->lose_cnt == 0;

    uint8_t stat;
    if (next && !rp->fetch_valid) {
        stat = BUS_STAT_BAD_NEXT;
    } else {
        if (next) {
            addr = rp->fetch_addr;
        }
        stat = rp_access(dev, addr, size, access, kernel, data);
    }

    if (access == MEM_FETCH) {
        rp->fetch_addr = addr + size;
        rp->fetch_valid = stat == BUS_STAT_OK && rp->fetch_addr != 0; // not past the end of the physical addresses
    }

    // The RP2040 has done the access, and the chip never learns it
    if (is_lost) {
        stat = BUS_STAT_LINK_ERROR;
    }

    if (dev->bus_reqs_cnt < BUS_REQS_MAX) {
        bus_req_t *req = &dev->bus_reqs[dev->bus_reqs_cnt++];
        req->access = access;
        req->next = next;
        req->kernel = kernel;
        req->size = size;
        req->addr = addr;
        req->data = stat == BUS_STAT_OK || access == MEM_WRITE ? *data : 0;
        if (size < 4) { // only `size` bytes of a value travel over the bus
            req->data &= ((uint32_t) 1 << (8 * size)) - 1;
        }
        req->stat = stat;
    }

    return stat;
}

/* Interrupts and Exceptions */

static void cpu_raise_irq(device_t *dev, uint8_t irq)
{
    assert(dev != NULL);

    /*
        Requests latch in the PIC's request register regardless of the mask,
        like the IRR of a real interrupt controller. Delivery happens at the
        next instruction boundary in `pic_deliver_irq`, so a request raised
        inside a `cli` window is delivered after `sti` instead of being lost.
    */
    dev->pic.irr |= (uint8_t) (1 << irq);
}

// Empties the instruction cache (see "Instruction Cache" at the `cr0` register)
static void cpu_flush_icache(cpu_t *cpu)
{
    assert(cpu != NULL);

    memset(cpu->ic_valid, 0, sizeof(cpu->ic_valid));
}

static void cpu_enter_trap(device_t *dev, uint8_t vec, uint32_t resume_ip, uint32_t cause)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    cpu->trapped = true;
    cpu_flush_icache(cpu);

    if (cpu->ctrl & CPU_CTRL_T) { // the trap record is not saved yet, so it must not be overwritten
        cpu->double_fault = true;
        return;
    }

    cpu->saved_ip = resume_ip;
    cpu->saved_flags = cpu->flags;
    cpu->cause = cause | vec;

    // The handler runs in kernel mode with interrupts disabled
    cpu->flags &= ~(CPU_FLAG_U | CPU_FLAG_I);
    cpu->ctrl |= CPU_CTRL_T;
    cpu->halted = false;
    cpu->fetch_run = false;

    uint32_t handler_addr;
    if (bus_access(dev, (uint32_t) vec * 4, 4, MEM_READ, true, false, &handler_addr) != BUS_STAT_OK) {
        cpu->double_fault = true;
        return;
    }
    cpu->ip = handler_addr;
}

static void pic_deliver_irq(device_t *dev)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    if (cpu->double_fault) {
        return;
    }

    // Delivery waits until interrupts are enabled and the trap record is free
    if (!(cpu->flags & CPU_FLAG_I) || (cpu->ctrl & CPU_CTRL_T)) {
        return;
    }

    // A level line is pending for as long as its device holds it
    uint8_t pending = dev->pic.irr;
    if (rp_get_irq_line(&dev->rp)) {
        pending |= (uint8_t) (1 << RP_IRQ);
    }

    uint8_t deliverable = pending & (uint8_t) ~dev->pic.imr;
    if (deliverable == 0) {
        return;
    }

    uint8_t irq = 0;
    while (!(deliverable & (1 << irq))) { // the lowest pending unmasked line wins
        ++irq;
    }

    // A halted CPU continues after its `hlt` when the handler returns
    uint32_t resume_ip = cpu->halted ? cpu->ip + CPU_INST_SIZE : cpu->ip;
    uint8_t vec = (uint8_t) (dev->pic.vec_off + irq); // the vector number wraps at 8 bits
    cpu_enter_trap(dev, vec, resume_ip, 0);

    dev->pic.irr &= (uint8_t) ~(1 << irq); // the delivered request is acknowledged
}

static void cpu_raise_excpt(device_t *dev, uint8_t excpt)
{
    assert(dev != NULL);

    /*
        Exceptions are not masked. The instruction that raised the exception
        has changed nothing, and its address is saved so that it can run again.
    */
    cpu_enter_trap(dev, excpt, dev->cpu.ip, 0);
}

static void cpu_raise_mem_excpt(device_t *dev, uint8_t excpt, uint32_t addr, mem_access_t access, uint32_t cause)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    if (access == MEM_WRITE) {
        cause |= CPU_CAUSE_W;
    }
    if (access == MEM_FETCH) {
        cause |= CPU_CAUSE_F;
    }
    if (cpu->flags & CPU_FLAG_U) {
        cause |= CPU_CAUSE_U;
    }

    if (!(cpu->ctrl & CPU_CTRL_T)) { // a double fault keeps the whole record of the first trap
        cpu->fault_addr = addr;
    }
    cpu_enter_trap(dev, excpt, cpu->ip, cause);
}

/* Memory Access */

static void cpu_flush_tlb(cpu_t *cpu)
{
    assert(cpu != NULL);

    for (size_t i = 0; i < CPU_TLB_SIZE; ++i) {
        cpu->tlb[i].valid = false;
    }
    cpu->tlb_next = 0;
}

static tlb_entry_t *cpu_find_tlb_entry(cpu_t *cpu, uint32_t vpn)
{
    assert(cpu != NULL);

    // The hardware compares the page number with all entries at once
    for (size_t i = 0; i < CPU_TLB_SIZE; ++i) {
        if (cpu->tlb[i].valid && cpu->tlb[i].vpn == vpn) {
            return &cpu->tlb[i];
        }
    }

    return NULL;
}

static void cpu_load_tlb_entry(cpu_t *cpu, uint32_t vpn, uint32_t pte)
{
    assert(cpu != NULL);

    if (!(pte & PTE_P)) { // an entry that is not present is not a translation
        return;
    }

    tlb_entry_t *entry = cpu_find_tlb_entry(cpu, vpn);
    if (entry == NULL) { // a translation of the same page is replaced in place, so a page never has two entries
        entry = &cpu->tlb[cpu->tlb_next];
        cpu->tlb_next = (uint8_t) ((cpu->tlb_next + 1) % CPU_TLB_SIZE);
    }

    entry->valid = true;
    entry->vpn = vpn;
    entry->pfn = pte >> 12;
    entry->writable = (pte & PTE_W) != 0;
    entry->user = (pte & PTE_U) != 0;
}

static bool cpu_is_refused(cpu_t *cpu, mem_access_t access, bool writable, bool user)
{
    assert(cpu != NULL);

    if ((cpu->flags & CPU_FLAG_U) && !user) {
        return true;
    }
    if (access == MEM_WRITE && !writable) {
        return true;
    }

    return false;
}

/*
    Reads one page table entry during the chip's own lookup. Returns false
    after raising a page fault if the entry cannot be used for the access.
*/
static bool cpu_read_pte(device_t *dev, uint32_t table_addr, uint32_t index, uint32_t level, uint32_t addr, mem_access_t access, uint32_t *pte)
{
    assert(dev != NULL);
    assert(pte != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t lvl = level << CPU_CAUSE_LVL_SHIFT;

    /*
        A table is in RAM, which is in the first 8 MB of the physical addresses.
        The chip does not read an entry anywhere else: a read of a device
        register in place of a table could change the device.
    */
    if (table_addr >= PT_ADDR_END) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_PAGE_FAULT, addr, access, lvl | CPU_CAUSE_M);
        return false;
    }

    // The chip reads the tables in kernel mode whatever the mode of the access is
    uint8_t stat = bus_access(dev, table_addr + index * 4, 4, MEM_READ, true, false, pte);
    if (stat != BUS_STAT_OK) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_PAGE_FAULT, addr, access, lvl | ((uint32_t) stat << CPU_CAUSE_STAT_SHIFT));
        return false;
    }

    if (!(*pte & PTE_P)) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_PAGE_FAULT, addr, access, lvl);
        return false;
    }
    if (cpu_is_refused(cpu, access, (*pte & PTE_W) != 0, (*pte & PTE_U) != 0)) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_PAGE_FAULT, addr, access, lvl | CPU_CAUSE_P);
        return false;
    }

    return true;
}

/*
    Turns the address of an access into a physical address (see the "Address
    Space" comment above). Returns false after raising an exception if the
    access is not allowed.
*/
static bool cpu_translate(device_t *dev, uint32_t addr, uint8_t size, mem_access_t access, uint32_t *phys_addr)
{
    assert(dev != NULL);
    assert(phys_addr != NULL);

    cpu_t *cpu = &dev->cpu;

    if (addr % size != 0) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_MISALIGNED, addr, access, 0);
        return false;
    }

    if (addr >= KERN_BASE) {
        if (cpu->flags & CPU_FLAG_U) {
            cpu_raise_mem_excpt(dev, CPU_EXCPT_BUS_ERROR, addr, access, 0);
            return false;
        }

        *phys_addr = addr - KERN_BASE;
        return true;
    }

    if (!(cpu->ctrl & CPU_CTRL_P)) {
        *phys_addr = addr;
        return true;
    }

    uint32_t vpn = addr >> 12;
    tlb_entry_t *entry = cpu_find_tlb_entry(cpu, vpn);
    if (entry == NULL) {
        if (cpu->ctrl & CPU_CTRL_S) { // the kernel looks the translation up and loads it through `cr7`
            cpu_raise_mem_excpt(dev, CPU_EXCPT_TLB_MISS, addr, access, 0);
            return false;
        }

        uint32_t top_pte;
        if (!cpu_read_pte(dev, cpu->pt_root, addr >> 22, 1, addr, access, &top_pte)) {
            return false;
        }

        uint32_t pte;
        if (!cpu_read_pte(dev, top_pte & 0xfffff000, (addr >> 12) & 0x3ff, 2, addr, access, &pte)) {
            return false;
        }

        // An access needs the permission in both entries
        cpu_load_tlb_entry(cpu, vpn, pte & (top_pte | ~(uint32_t) (PTE_W | PTE_U)));
        entry = cpu_find_tlb_entry(cpu, vpn);

        // The new translation may have taken the place of the one that the instructions are fetched through
        cpu->fetch_run = false;
    }

    if (cpu_is_refused(cpu, access, entry->writable, entry->user)) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_PAGE_FAULT, addr, access, CPU_CAUSE_P);
        return false;
    }

    *phys_addr = (entry->pfn << 12) | (addr & 0xfff);
    return true;
}

/*
    Reads or writes memory at the address of the current mode, or fetches an
    instruction there. Returns false after raising an exception if the access
    has failed.
*/
static bool cpu_access_mem(device_t *dev, uint32_t addr, uint8_t size, mem_access_t access, uint32_t *data)
{
    assert(dev != NULL);
    assert(data != NULL);

    cpu_t *cpu = &dev->cpu;

    // See "Instruction Cache" at the `cr0` register
    uint32_t ic_line = (addr / 32) % CPU_IC_LINES;
    uint32_t ic_word = (addr / 4) % 8;
    if (access == MEM_FETCH && (cpu->ctrl & CPU_CTRL_C) && addr % 4 == 0 &&
        cpu->ic_tag[ic_line] == addr / 32 && (cpu->ic_valid[ic_line] & (1u << ic_word))) {
        *data = cpu->ic_data[ic_line * 8 + ic_word];
        cpu->fetch_run = false; // the RP2040 has not seen this fetch
        return true;
    }

    uint32_t phys_addr;
    if (!cpu_translate(dev, addr, size, access, &phys_addr)) {
        return false;
    }

    // See "Sequential Fetch" at the `cr0` register
    bool next = access == MEM_FETCH && cpu->fetch_run && !(cpu->ctrl & CPU_CTRL_F);

    uint8_t stat = bus_access(dev, phys_addr, size, access, !(cpu->flags & CPU_FLAG_U), next, data);
    if (stat != BUS_STAT_OK) {
        cpu_raise_mem_excpt(dev, CPU_EXCPT_BUS_ERROR, addr, access, (uint32_t) stat << CPU_CAUSE_STAT_SHIFT);
        return false;
    }

    if (access == MEM_FETCH) { // the page after this one may be anywhere in memory
        cpu->fetch_run = (addr & (PAGE_SIZE - 1)) != PAGE_SIZE - CPU_INST_SIZE;

        if (cpu->ctrl & CPU_CTRL_C) {
            if (cpu->ic_tag[ic_line] != addr / 32) { // a word of another block takes the line over
                cpu->ic_tag[ic_line] = addr / 32;
                cpu->ic_valid[ic_line] = 0;
            }
            cpu->ic_valid[ic_line] |= (uint8_t) (1u << ic_word);
            cpu->ic_data[ic_line * 8 + ic_word] = *data;
        }
    }

    return true;
}

/*
    Makes `addr` the address of the next instruction. Every write to `ip` ends
    a run of sequential fetches, whatever the address is.
*/
static void cpu_jump(cpu_t *cpu, uint32_t addr)
{
    assert(cpu != NULL);

    cpu->next_ip = addr;
    cpu->fetch_run = false;
}

/* Register Access */

static uint32_t cpu_get_reg(cpu_t *cpu, uint8_t reg)
{
    assert(cpu != NULL);

    switch (reg) {
        case CPU_REG_R0:    return cpu->r0;
        case CPU_REG_R1:    return cpu->r1;
        case CPU_REG_R2:    return cpu->r2;
        case CPU_REG_R3:    return cpu->r3;
        case CPU_REG_R4:    return cpu->r4;
        case CPU_REG_R5:    return cpu->r5;
        case CPU_REG_R6:    return cpu->r6;
        case CPU_REG_R7:    return cpu->r7;
        case CPU_REG_FP:    return cpu->fp;
        case CPU_REG_SP:    return (cpu->flags & CPU_FLAG_U) ? cpu->usp : cpu->ksp; // every mode has its own stack pointer
        case CPU_REG_LR:    return cpu->lr;
        case CPU_REG_IP:    return cpu->ip;
        case CPU_REG_FLAGS: return cpu->flags;
        default:            assert(false); return 0;
    }
}

static void cpu_set_reg(cpu_t *cpu, uint8_t reg, uint32_t val)
{
    assert(cpu != NULL);

    switch (reg) {
        case CPU_REG_R0: cpu->r0 = val; break;
        case CPU_REG_R1: cpu->r1 = val; break;
        case CPU_REG_R2: cpu->r2 = val; break;
        case CPU_REG_R3: cpu->r3 = val; break;
        case CPU_REG_R4: cpu->r4 = val; break;
        case CPU_REG_R5: cpu->r5 = val; break;
        case CPU_REG_R6: cpu->r6 = val; break;
        case CPU_REG_R7: cpu->r7 = val; break;
        case CPU_REG_FP: cpu->fp = val; break;
        case CPU_REG_SP:
            if (cpu->flags & CPU_FLAG_U) {
                cpu->usp = val;
            } else {
                cpu->ksp = val;
            }
            break;
        case CPU_REG_LR: cpu->lr = val; break;
        case CPU_REG_IP: cpu_jump(cpu, val); break; // a write to `ip` is a jump
        case CPU_REG_FLAGS: // the I and U flags cannot be changed by an ordinary write
            cpu->flags = (cpu->flags & ~(uint32_t) CPU_FLAGS_ARITH) | (val & CPU_FLAGS_ARITH);
            break;
        default: assert(false);
    }
}

static uint32_t cpu_get_ctrl_reg(cpu_t *cpu, uint8_t ctrl_reg)
{
    assert(cpu != NULL);

    switch (ctrl_reg) {
        case CPU_CTRL_REG_CTRL:        return cpu->ctrl;
        case CPU_CTRL_REG_SAVED_IP:    return cpu->saved_ip;
        case CPU_CTRL_REG_FAULT_ADDR:  return cpu->fault_addr;
        case CPU_CTRL_REG_PT_ROOT:     return cpu->pt_root;
        case CPU_CTRL_REG_SAVED_FLAGS: return cpu->saved_flags;
        case CPU_CTRL_REG_CAUSE:       return cpu->cause;
        case CPU_CTRL_REG_USP:         return cpu->usp;
        case CPU_CTRL_REG_TLB_LOAD:    return 0;
        case CPU_CTRL_REG_DBG_LOCK:    return cpu->dbg_locked;
        default:                       assert(false); return 0;
    }
}

static void cpu_set_ctrl_reg(cpu_t *cpu, uint8_t ctrl_reg, uint32_t val)
{
    assert(cpu != NULL);

    switch (ctrl_reg) {
        case CPU_CTRL_REG_CTRL:
            // The TLB holds translations made under the old settings
            if ((cpu->ctrl ^ val) & (CPU_CTRL_P | CPU_CTRL_S)) {
                cpu_flush_tlb(cpu);
            }
            cpu->ctrl = val & CPU_CTRL_ALL;
            break;
        case CPU_CTRL_REG_SAVED_IP:
            cpu->saved_ip = val;
            break;
        case CPU_CTRL_REG_FAULT_ADDR:
            cpu->fault_addr = val;
            break;
        case CPU_CTRL_REG_PT_ROOT:
            cpu->pt_root = val & 0xfffff000;
            cpu_flush_tlb(cpu); // even if the same value is written again
            break;
        case CPU_CTRL_REG_SAVED_FLAGS:
            cpu->saved_flags = val & CPU_FLAGS_ALL;
            break;
        case CPU_CTRL_REG_CAUSE:
            cpu->cause = val & CPU_CAUSE_ALL;
            break;
        case CPU_CTRL_REG_USP:
            cpu->usp = val;
            break;
        case CPU_CTRL_REG_TLB_LOAD:
            if (cpu->fault_addr < KERN_BASE) { // only the lower half of the address space is translated
                cpu_load_tlb_entry(cpu, cpu->fault_addr >> 12, val);
            }
            break;
        case CPU_CTRL_REG_DBG_LOCK:
            if (val == CPU_DBG_LOCK_KEY) { // no value opens the port again
                cpu->dbg_locked = true;
            }
            break;
        default: assert(false);
    }

    /*
        Three of the registers decide where the next instruction is in
        physical memory. The chip does not tell them from the others: a write
        to any control register ends a run of sequential fetches.
    */
    cpu->fetch_run = false;
    cpu_flush_icache(cpu);
}

static void cpu_set_flag(cpu_t *cpu, uint32_t flag, bool set)
{
    assert(cpu != NULL);

    if (set) {
        cpu->flags |= flag;
    } else {
        cpu->flags &= ~flag;
    }
}

/* CPU Instructions */

/*
    In the functions below `src` is the value of the first operand: register A
    or, in the immediate form, the sign-extended immediate value. The current
    instruction is at `ip`; the CPU moves to `next_ip` when the instruction
    ends without an exception.
*/

/* System Instructions */

static void cpu_hlt(device_t *dev)
{
    assert(dev != NULL);

    /*
        Stops the CPU from progressing by not changing the instruction pointer.
        It is important to ensure that this instruction has a code 0x00 to
        prevent uninitialized code doing harm.

        `hlt` waits for interrupts: the devices keep stepping, so with the
        interrupt flag set an unmasked request still redirects the CPU into
        its handler, and `iret` returns to the instruction after the `hlt`.
        With interrupts disabled the CPU is parked for good.
    */
    dev->cpu.halted = true;
    dev->cpu.next_ip = dev->cpu.ip;
}

static void cpu_nop(device_t *dev)
{
    assert(dev != NULL);

    // Do nothing
}

static void cpu_cli(device_t *dev)
{
    assert(dev != NULL);

    dev->cpu.flags &= ~CPU_FLAG_I;
}

static void cpu_sti(device_t *dev)
{
    assert(dev != NULL);

    dev->cpu.flags |= CPU_FLAG_I;
}

static void cpu_iret(device_t *dev)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    cpu_jump(cpu, cpu->saved_ip);
    cpu->flags = cpu->saved_flags; // including the I and U flags, so `sp` may name the other stack pointer from now on
    cpu->ctrl &= ~CPU_CTRL_T;
    cpu_flush_icache(cpu);
}

static void cpu_int(device_t *dev, uint8_t num)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    // Unlike an exception, `int` is done once the handler is entered, so the handler returns to the next instruction
    cpu_enter_trap(dev, CPU_EXCPT_INT, cpu->ip + CPU_INST_SIZE, (uint32_t) num << CPU_CAUSE_INT_SHIFT);

    if (!cpu->double_fault) { // for the same reason `int` retires as any other instruction, with the handler as its jump target
        cpu->next_ip = cpu->ip;
        cpu->trapped = false;
    }
}

static void cpu_inb(device_t *dev, uint32_t src_port, uint8_t reg_dst)
{
    assert(dev != NULL);

    uint8_t port = (uint8_t) src_port; // the port number is the low byte of the operand
    uint8_t val;

    if (port == PIC_DATA_IO_PORT) {
        val = dev->pic.imr;
    } else if (port == PIT_DATA_IO_PORT) {
        val = dev->pit.cnt_low;
    } else if (port == KBDC_DATA_IO_PORT) {
        val = dev->kbdc.scancode;
        dev->kbdc.status &= ~KBDC_FLAG_WAITING; // the byte has been read
    } else if (port == KBDC_STAT_IO_PORT) {
        val = dev->kbdc.status;
    } else {
        val = 0x00; // an unmapped port reads as zero, matching the hardware I/O read mux
    }

    cpu_set_reg(&dev->cpu, reg_dst, val);
}

static void cpu_outb(device_t *dev, uint32_t src, uint8_t dst_port_in_reg)
{
    assert(dev != NULL);

    uint8_t port = (uint8_t) cpu_get_reg(&dev->cpu, dst_port_in_reg); // the port number is the low byte of the register
    uint8_t val = (uint8_t) src;

    if (port == PIC_CMD_IO_PORT) {
        dev->pic.state = (pic_state_t) val;
    } else if (port == PIT_CMD_IO_PORT) {
        dev->pit.state = (pit_state_t) val;
    } else if (port == KBDC_STAT_IO_PORT) {
        if (val & KBDC_FLAG_OVERRUN) { // the driver has taken note of the lost byte
            dev->kbdc.status &= ~KBDC_FLAG_OVERRUN;
        }
    }

    if (port == PIC_DATA_IO_PORT) {
        if (dev->pic.state == PIC_AWAITS_IMR) {
            dev->pic.imr = val;
        } else if (dev->pic.state == PIC_AWAITS_VEC_OFF) {
            dev->pic.vec_off = val;
        }
    } else if (port == PIT_DATA_IO_PORT) {
        if (dev->pit.state == PIT_AWAITS_FREQ_DIV_HIGH) {
            dev->pit.freq_div_high = val;
        } else if (dev->pit.state == PIT_AWAITS_FREQ_DIV_LOW) {
            dev->pit.freq_div_low = val;
            dev->pit.cnt_high = dev->pit.freq_div_high;
            dev->pit.cnt_low  = dev->pit.freq_div_low;
        }
    }
}

static void cpu_mfc(device_t *dev, uint8_t ctrl_reg_src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_set_reg(&dev->cpu, reg_dst, cpu_get_ctrl_reg(&dev->cpu, ctrl_reg_src));
}

static void cpu_mtc(device_t *dev, uint32_t src, uint8_t ctrl_reg_dst)
{
    assert(dev != NULL);

    cpu_set_ctrl_reg(&dev->cpu, ctrl_reg_dst, src);
}

/* Load and Store Instructions */

static void cpu_mov(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_set_reg(&dev->cpu, reg_dst, src);
}

static void cpu_lih(device_t *dev, uint16_t imm, uint8_t reg_dst)
{
    assert(dev != NULL);

    /*
        `li` followed by `lih` loads any 32-bit value in two instructions:
        `li` sets the lower half (and extends its sign into the upper half),
        `lih` then replaces the upper half. Neither changes the flags.
    */
    uint32_t res = ((uint32_t) imm << 16) | (cpu_get_reg(&dev->cpu, reg_dst) & 0xffff);
    cpu_set_reg(&dev->cpu, reg_dst, res);
}

static void cpu_push(device_t *dev, uint32_t src)
{
    assert(dev != NULL);

    // The stack pointer moves only after the store has succeeded, so `push sp` stores the value before the decrement.
    uint32_t addr = cpu_get_reg(&dev->cpu, CPU_REG_SP) - 4;
    if (!cpu_access_mem(dev, addr, 4, MEM_WRITE, &src)) {
        return;
    }

    cpu_set_reg(&dev->cpu, CPU_REG_SP, addr);
}

static void cpu_pop(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    uint32_t addr = cpu_get_reg(&dev->cpu, CPU_REG_SP);
    uint32_t val;
    if (!cpu_access_mem(dev, addr, 4, MEM_READ, &val)) {
        return;
    }

    cpu_set_reg(&dev->cpu, CPU_REG_SP, addr + 4);
    cpu_set_reg(&dev->cpu, reg_dst, val); // a pop into `sp` keeps the popped value
}

static void cpu_sr(device_t *dev, uint32_t src, uint8_t reg_with_addr_of_dst, uint16_t disp, uint8_t size)
{
    assert(dev != NULL);

    // The displacement is a signed number of bytes
    uint32_t addr = cpu_get_reg(&dev->cpu, reg_with_addr_of_dst) + (uint32_t) (int16_t) disp;
    cpu_access_mem(dev, addr, size, MEM_WRITE, &src); // only the low `size` bytes are stored
}

static void cpu_lr(device_t *dev, uint8_t reg_with_addr_of_src, uint16_t disp, uint8_t size, bool is_signed, uint8_t reg_dst)
{
    assert(dev != NULL);

    // The displacement is a signed number of bytes
    uint32_t addr = cpu_get_reg(&dev->cpu, reg_with_addr_of_src) + (uint32_t) (int16_t) disp;
    uint32_t val;
    if (!cpu_access_mem(dev, addr, size, MEM_READ, &val)) {
        return;
    }

    if (is_signed && size == 1) {
        val = (uint32_t) (int8_t) val;
    } else if (is_signed && size == 2) {
        val = (uint32_t) (int16_t) val;
    }

    cpu_set_reg(&dev->cpu, reg_dst, val);
}

/* Arithmetic Instructions */

static void cpu_add(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint64_t res = (uint64_t) dst + src;

    cpu_set_flag(cpu, CPU_FLAG_C, res >> 32);
    cpu_set_flag(cpu, CPU_FLAG_Z, (uint32_t) res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    // Signed overflow: both operands have the same sign, and the result has the other one
    cpu_set_flag(cpu, CPU_FLAG_O, ~(dst ^ src) & (dst ^ res) & 0x80000000);

    cpu_set_reg(cpu, reg_dst, (uint32_t) res); // assign the result last so a `flags` destination keeps it
}

static void cpu_adc(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint32_t carry = cpu->flags & CPU_FLAG_C;
    uint64_t res = (uint64_t) dst + src + carry;

    cpu_set_flag(cpu, CPU_FLAG_C, res >> 32);
    cpu_set_flag(cpu, CPU_FLAG_Z, (uint32_t) res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, ~(dst ^ src) & (dst ^ res) & 0x80000000);

    cpu_set_reg(cpu, reg_dst, (uint32_t) res);
}

static void cpu_sub(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint64_t res = (uint64_t) dst - src;

    cpu_set_flag(cpu, CPU_FLAG_C, res >> 32); // a borrow reaches the bits above bit 31
    cpu_set_flag(cpu, CPU_FLAG_Z, (uint32_t) res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    // Signed overflow: the operands have different signs, and the result has the sign of the subtrahend
    cpu_set_flag(cpu, CPU_FLAG_O, (dst ^ src) & (dst ^ res) & 0x80000000);

    cpu_set_reg(cpu, reg_dst, (uint32_t) res);
}

static void cpu_cmp(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint64_t res = (uint64_t) dst - src;

    cpu_set_flag(cpu, CPU_FLAG_C, res >> 32);
    cpu_set_flag(cpu, CPU_FLAG_Z, (uint32_t) res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, (dst ^ src) & (dst ^ res) & 0x80000000);
}

static void cpu_sbb(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint32_t borrow = cpu->flags & CPU_FLAG_C;
    uint64_t res = (uint64_t) dst - src - borrow;

    cpu_set_flag(cpu, CPU_FLAG_C, res >> 32);
    cpu_set_flag(cpu, CPU_FLAG_Z, (uint32_t) res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, (dst ^ src) & (dst ^ res) & 0x80000000);

    cpu_set_reg(cpu, reg_dst, (uint32_t) res);
}

static void cpu_mul(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    // The low 32 bits of a product are the same for signed and unsigned numbers. The flags do not change.
    cpu_set_reg(&dev->cpu, reg_dst, cpu_get_reg(&dev->cpu, reg_dst) * src);
}

static void cpu_dot4(device_t *dev, uint8_t reg_src1, uint8_t reg_src2, uint8_t reg_dst)
{
    assert(dev != NULL);

    /*
        A dot product of two vectors of four signed bytes each, added to the
        destination: the step at the heart of a small neural network. All
        three registers are read before the destination is written, and the
        flags do not change.
    */
    cpu_t *cpu = &dev->cpu;
    uint32_t src1 = cpu_get_reg(cpu, reg_src1);
    uint32_t src2 = cpu_get_reg(cpu, reg_src2);
    uint32_t res = cpu_get_reg(cpu, reg_dst);

    for (size_t i = 0; i < 4; ++i) {
        int8_t byte1 = (int8_t) (src1 >> (8 * i));
        int8_t byte2 = (int8_t) (src2 >> (8 * i));
        res += (uint32_t) ((int32_t) byte1 * (int32_t) byte2);
    }

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_popcnt(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    /*
        The number of bits that are set. A network whose weights and inputs
        are one bit each, 1 for +1 and 0 for -1, multiplies 32 of them with
        one `xor`: a bit of the result is set where the product is -1. With
        `count` such bits the sum of the 32 products is `32 - 2 * count`.
        The flags do not change.
    */
    uint32_t count = 0;
    for (size_t i = 0; i < 32; ++i) {
        count += (src >> i) & 1;
    }

    cpu_set_reg(&dev->cpu, reg_dst, count);
}

static void cpu_ssat(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    // The sum of a neuron on its way to a byte: a value outside the range of a signed byte becomes the nearest end of the range. The flags do not change.
    int32_t val = (int32_t) cpu_get_reg(&dev->cpu, reg_dst);
    if (val < -128) {
        val = -128;
    } else if (val > 127) {
        val = 127;
    }

    cpu_set_reg(&dev->cpu, reg_dst, (uint32_t) val);
}

static void cpu_usat(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    // As `ssat`, but a negative value becomes 0: the rule of a rectified linear unit (ReLU)
    int32_t val = (int32_t) cpu_get_reg(&dev->cpu, reg_dst);
    if (val < 0) {
        val = 0;
    } else if (val > 127) {
        val = 127;
    }

    cpu_set_reg(&dev->cpu, reg_dst, (uint32_t) val);
}

/* Instructions of the Array of the Dot Products (see "Array of the Dot Products" above) */

static bool cpu_is_dot_reg(uint8_t dot_reg)
{
    return dot_reg < CPU_DOT_REGS && (dot_reg & 3) < CPU_DOT_ROWS; // the array has `CPU_DOT_ROWS` of the four rows that can be named
}

static uint32_t *cpu_get_dot_reg(cpu_t *cpu, uint8_t dot_reg)
{
    assert(cpu != NULL);
    assert(cpu_is_dot_reg(dot_reg));

    return dot_reg < CPU_DOT_REG_X0 ? &cpu->dot_w[dot_reg - CPU_DOT_REG_W0] : &cpu->dot_x[dot_reg - CPU_DOT_REG_X0];
}

// The sum of the four products of a row
static int32_t cpu_dot_row(cpu_t *cpu, size_t row)
{
    assert(cpu != NULL);

    int32_t sum = 0;
    for (size_t i = 0; i < 4; ++i) {
        int8_t weight = (int8_t) (cpu->dot_w[row] >> (8 * i));
        int8_t input  = (int8_t) (cpu->dot_x[row] >> (8 * i));
        sum += (int32_t) weight * (int32_t) input;
    }

    return sum;
}

static void cpu_mfa(device_t *dev, uint8_t dot_reg_src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_set_reg(&dev->cpu, reg_dst, *cpu_get_dot_reg(&dev->cpu, dot_reg_src));
}

static void cpu_mta(device_t *dev, uint32_t src, uint8_t dot_reg_dst)
{
    assert(dev != NULL);

    *cpu_get_dot_reg(&dev->cpu, dot_reg_dst) = src;
}

static void cpu_dotv(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t res = cpu_get_reg(cpu, reg_dst);

    for (size_t row = 0; row < CPU_DOT_ROWS; ++row) {
        res += (uint32_t) cpu_dot_row(cpu, row);
    }

    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_dotm(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    for (size_t row = 0; row < CPU_DOT_ROWS; ++row) {
        uint8_t reg = reg_dst + (uint8_t) row;
        cpu_set_reg(cpu, reg, cpu_get_reg(cpu, reg) + (uint32_t) cpu_dot_row(cpu, row));
    }
}

static void cpu_learn(device_t *dev, uint32_t src)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    for (size_t row = 0; row < CPU_DOT_ROWS; ++row) {
        int8_t error = (int8_t) (src >> (8 * row));
        uint32_t res = 0;

        for (size_t i = 0; i < 4; ++i) {
            int8_t weight = (int8_t) (cpu->dot_w[row] >> (8 * i));
            int8_t input  = (int8_t) (cpu->dot_x[row] >> (8 * i));

            // The chip computes `weight * 128 + 64 + error * input` and takes the bits above the low seven: a division by 128 that rounds down
            int32_t sum = (int32_t) weight * 128 + 64 + (int32_t) error * (int32_t) input;
            sum = sum >= 0 ? sum / 128 : -((127 - sum) / 128);
            if (sum < -128) {
                sum = -128;
            } else if (sum > 127) {
                sum = 127;
            }

            res |= (uint32_t) (uint8_t) sum << (8 * i);
        }

        cpu->dot_w[row] = res;
    }
}

static void cpu_xsh(device_t *dev, uint32_t src)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint8_t in = (uint8_t) src;

    for (size_t row = 0; row < CPU_DOT_ROWS; ++row) {
        uint8_t out = (uint8_t) (cpu->dot_x[row] >> 24); // the byte of the last cell goes to the next row
        cpu->dot_x[row] = (cpu->dot_x[row] << 8) | in;
        in = out;
    }
}

/* Logic Instructions */

static void cpu_not(device_t *dev, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_set_reg(&dev->cpu, reg_dst, ~cpu_get_reg(&dev->cpu, reg_dst));
}

static void cpu_and(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t res = cpu_get_reg(cpu, reg_dst) & src;

    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_test(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t res = cpu_get_reg(cpu, reg_dst) & src;

    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);
}

static void cpu_or(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t res = cpu_get_reg(cpu, reg_dst) | src;

    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_xor(device_t *dev, uint32_t src, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t res = cpu_get_reg(cpu, reg_dst) ^ src;

    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);

    cpu_set_reg(cpu, reg_dst, res);
}

/* Shift Instructions */

/*
    The shift count is the low 5 bits of the first operand, 0 to 31. A zero
    shift writes nothing: the register and the flags stay as they are.
*/

static void cpu_shl(device_t *dev, uint32_t src_shift, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t shift = src_shift & 31;
    if (shift == 0) {
        return;
    }

    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint32_t res = dst << shift;
    uint32_t carry = (dst >> (32 - shift)) & 1; // the last bit shifted out

    cpu_set_flag(cpu, CPU_FLAG_C, carry);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, (res >> 31) != carry);

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_shr(device_t *dev, uint32_t src_shift, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t shift = src_shift & 31;
    if (shift == 0) {
        return;
    }

    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint32_t res = dst >> shift;
    uint32_t carry = (dst >> (shift - 1)) & 1; // the last bit shifted out

    cpu_set_flag(cpu, CPU_FLAG_C, carry);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, dst & 0x80000000);

    cpu_set_reg(cpu, reg_dst, res);
}

static void cpu_sar(device_t *dev, uint32_t src_shift, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;
    uint32_t shift = src_shift & 31;
    if (shift == 0) {
        return;
    }

    uint32_t dst = cpu_get_reg(cpu, reg_dst);
    uint32_t res = dst >> shift;
    if (dst & 0x80000000) {
        res |= ~(0xffffffff >> shift); // the vacated upper bits take the sign bit
    }
    uint32_t carry = (dst >> (shift - 1)) & 1; // the last bit shifted out

    cpu_set_flag(cpu, CPU_FLAG_C, carry);
    cpu_set_flag(cpu, CPU_FLAG_Z, res == 0);
    cpu_set_flag(cpu, CPU_FLAG_S, res & 0x80000000);
    cpu_set_flag(cpu, CPU_FLAG_O, false);

    cpu_set_reg(cpu, reg_dst, res);
}

/* Jump Instructions */

static bool cpu_is_cond_true(cpu_t *cpu, uint8_t cond)
{
    assert(cpu != NULL);

    bool c = (cpu->flags & CPU_FLAG_C) != 0;
    bool z = (cpu->flags & CPU_FLAG_Z) != 0;
    bool s = (cpu->flags & CPU_FLAG_S) != 0;
    bool o = (cpu->flags & CPU_FLAG_O) != 0;

    switch (cond) {
        case CPU_COND_E:  return z;
        case CPU_COND_NE: return !z;
        case CPU_COND_L:  return s != o;
        case CPU_COND_GE: return s == o;
        case CPU_COND_LE: return z || s != o;
        case CPU_COND_G:  return !z && s == o;
        case CPU_COND_B:  return c;
        case CPU_COND_AE: return !c;
        case CPU_COND_BE: return c || z;
        case CPU_COND_A:  return !c && !z;
        default:          assert(false); return false;
    }
}

static void cpu_jmp(device_t *dev, uint32_t addr_of_dst)
{
    assert(dev != NULL);

    cpu_jump(&dev->cpu, addr_of_dst);
}

static void cpu_jcc(device_t *dev, uint8_t cond, uint32_t addr_of_dst)
{
    assert(dev != NULL);

    if (cpu_is_cond_true(&dev->cpu, cond)) {
        cpu_jump(&dev->cpu, addr_of_dst);
    }
}

static void cpu_setcc(device_t *dev, uint8_t cond, uint8_t reg_dst)
{
    assert(dev != NULL);

    cpu_set_reg(&dev->cpu, reg_dst, cpu_is_cond_true(&dev->cpu, cond) ? 1 : 0);
}

/* Subroutine Control Instructions */

static void cpu_call(device_t *dev, uint32_t addr_of_dst)
{
    assert(dev != NULL);

    dev->cpu.lr = dev->cpu.ip + CPU_INST_SIZE;
    cpu_jump(&dev->cpu, addr_of_dst);
}

static void cpu_ret(device_t *dev)
{
    assert(dev != NULL);

    cpu_jump(&dev->cpu, dev->cpu.lr);
}

/* Device Emulation */

/*
    Finds the encoding of an instruction word. Returns NULL if the word is not
    a valid instruction: its opcode is undefined, a register number is out of
    range, or a field that the instruction does not use is not zero.
*/
static const inst_enc_t *cpu_decode(uint32_t inst)
{
    uint8_t  opcode = inst & 0xff;
    uint8_t  reg_b  = (inst >> 8) & 0xf;
    uint8_t  reg_a  = (inst >> 12) & 0xf;
    uint16_t imm    = inst >> 16;

    const inst_enc_t *enc = NULL;
    for (size_t i = 0; i < CPU_INST; ++i) {
        if (INST_ENC_TABLE[i].code == opcode) {
            enc = &INST_ENC_TABLE[i];
            break;
        }
    }
    if (enc == NULL) {
        return NULL;
    }

    bool valid = false;
    switch (enc->fmt) {
        case CPU_FMT_NONE:  valid = reg_a == 0 && reg_b == 0 && imm == 0; break;
        case CPU_FMT_A:     valid = reg_a < CPU_REGS && reg_b == 0 && imm == 0; break;
        case CPU_FMT_B:     valid = reg_a == 0 && reg_b < CPU_REGS && imm == 0; break;
        case CPU_FMT_AB:    valid = reg_a < CPU_REGS && reg_b < CPU_REGS && imm == 0; break;
        case CPU_FMT_I:     valid = reg_a == 0 && reg_b == 0; break;
        case CPU_FMT_IB:    valid = reg_a == 0 && reg_b < CPU_REGS; break;
        case CPU_FMT_REL:   valid = reg_a == 0 && reg_b == 0; break;
        case CPU_FMT_LOAD:  valid = reg_a < CPU_REGS && reg_b < CPU_REGS; break;
        case CPU_FMT_STORE: valid = reg_a < CPU_REGS && reg_b < CPU_REGS; break;
        case CPU_FMT_ABC:   valid = reg_a < CPU_REGS && reg_b < CPU_REGS && imm < CPU_REGS; break;
        case CPU_FMT_CB:    valid = reg_a < CPU_CTRL_REGS && reg_b < CPU_REGS && imm == 0; break;
        case CPU_FMT_AC:    valid = reg_a < CPU_REGS && reg_b < CPU_CTRL_REGS && imm == 0; break;
        case CPU_FMT_DB:    valid = cpu_is_dot_reg(reg_a) && reg_b < CPU_REGS && imm == 0; break;
        case CPU_FMT_AD:    valid = reg_a < CPU_REGS && cpu_is_dot_reg(reg_b) && imm == 0; break;
    }
    if (opcode == (CPU_MNEM_INT | CPU_OPC_IMM) && imm > 0xff) { // the cause register has 8 bits for the number
        valid = false;
    }
    if (opcode == CPU_MNEM_DOTM && reg_b + CPU_DOT_ROWS - 1 > CPU_REG_R7) { // every row adds to a register from `r0` to `r7`
        valid = false;
    }

    return valid ? enc : NULL;
}

static void cpu_step(device_t *dev)
{
    assert(dev != NULL);

    cpu_t *cpu = &dev->cpu;

    cpu->retired = false;
    dev->bus_reqs_cnt = 0;

    if (cpu->halted || cpu->double_fault) { // only an interrupt wakes a halted CPU, and only a reset ends a double fault
        return;
    }

    ++cpu->cycles;

    cpu->trapped = false;
    cpu->next_ip = cpu->ip + CPU_INST_SIZE;

    uint32_t inst;
    if (!cpu_access_mem(dev, cpu->ip, CPU_INST_SIZE, MEM_FETCH, &inst)) {
        return;
    }
    cpu->inst = inst;

    const inst_enc_t *enc = cpu_decode(inst);
    if (enc == NULL) {
        cpu_raise_excpt(dev, CPU_EXCPT_INVALID_INSTR);
        return;
    }
    if (enc->privileged && (cpu->flags & CPU_FLAG_U)) {
        cpu_raise_excpt(dev, CPU_EXCPT_PRIVILEGED);
        return;
    }

    uint8_t  opcode = inst & 0xff;

    // The registers of the array are the kernel's until it sets the A bit of `cr0`
    bool uses_dot_regs = opcode >= CPU_MNEM_MFA && opcode <= CPU_MNEM_XSH;
    if (uses_dot_regs && !(cpu->ctrl & CPU_CTRL_A) && (cpu->flags & CPU_FLAG_U)) {
        cpu_raise_excpt(dev, CPU_EXCPT_PRIVILEGED);
        return;
    }

    uint8_t  reg_b  = (inst >> 8) & 0xf;
    uint8_t  reg_a  = (inst >> 12) & 0xf;
    uint16_t imm    = inst >> 16;

    // The first operand: register A or, in the immediate form, the sign-extended immediate value
    uint32_t src = 0;
    if (opcode & CPU_OPC_IMM) {
        src = (uint32_t) (int16_t) imm;
    } else if (enc->fmt == CPU_FMT_A || enc->fmt == CPU_FMT_AB || enc->fmt == CPU_FMT_STORE || enc->fmt == CPU_FMT_AC || enc->fmt == CPU_FMT_AD) {
        src = cpu_get_reg(cpu, reg_a);
    }

    // The target of a jump or a call: the first operand or, in the immediate form, a number of instructions from this one
    uint32_t target = src;
    if (enc->fmt == CPU_FMT_REL) {
        target = cpu->ip + src * CPU_INST_SIZE;
    }

    uint8_t op = opcode & ~CPU_OPC_IMM;
    switch (op) {
        case CPU_MNEM_HLT:  cpu_hlt(dev); break;
        case CPU_MNEM_NOP:  cpu_nop(dev); break;
        case CPU_MNEM_CLI:  cpu_cli(dev); break;
        case CPU_MNEM_STI:  cpu_sti(dev); break;
        case CPU_MNEM_IRET: cpu_iret(dev); break;
        case CPU_MNEM_INT:  cpu_int(dev, (uint8_t) imm); break;
        case CPU_MNEM_INB:  cpu_inb(dev, src, reg_b); break;
        case CPU_MNEM_OUTB: cpu_outb(dev, src, reg_b); break;
        case CPU_MNEM_MFC:  cpu_mfc(dev, reg_a, reg_b); break;
        case CPU_MNEM_MTC:  cpu_mtc(dev, src, reg_b); break;

        case CPU_MNEM_MOV:  cpu_mov(dev, src, reg_b); break;
        case CPU_MNEM_LIH:  cpu_lih(dev, imm, reg_b); break;
        case CPU_MNEM_PUSH: cpu_push(dev, src); break;
        case CPU_MNEM_POP:  cpu_pop(dev, reg_b); break;
        case CPU_MNEM_SR:   cpu_sr(dev, src, reg_b, imm, 4); break;
        case CPU_MNEM_SRH:  cpu_sr(dev, src, reg_b, imm, 2); break;
        case CPU_MNEM_SRB:  cpu_sr(dev, src, reg_b, imm, 1); break;
        case CPU_MNEM_LR:   cpu_lr(dev, reg_a, imm, 4, false, reg_b); break;
        case CPU_MNEM_LRH:  cpu_lr(dev, reg_a, imm, 2, true,  reg_b); break;
        case CPU_MNEM_LRB:  cpu_lr(dev, reg_a, imm, 1, true,  reg_b); break;
        case CPU_MNEM_LRHU: cpu_lr(dev, reg_a, imm, 2, false, reg_b); break;
        case CPU_MNEM_LRBU: cpu_lr(dev, reg_a, imm, 1, false, reg_b); break;

        case CPU_MNEM_ADD:  cpu_add(dev, src, reg_b); break;
        case CPU_MNEM_ADC:  cpu_adc(dev, src, reg_b); break;
        case CPU_MNEM_SUB:  cpu_sub(dev, src, reg_b); break;
        case CPU_MNEM_SBB:  cpu_sbb(dev, src, reg_b); break;
        case CPU_MNEM_CMP:  cpu_cmp(dev, src, reg_b); break;
        case CPU_MNEM_AND:  cpu_and(dev, src, reg_b); break;
        case CPU_MNEM_TEST: cpu_test(dev, src, reg_b); break;
        case CPU_MNEM_OR:   cpu_or(dev, src, reg_b); break;
        case CPU_MNEM_XOR:  cpu_xor(dev, src, reg_b); break;
        case CPU_MNEM_NOT:  cpu_not(dev, reg_b); break;
        case CPU_MNEM_SHL:  cpu_shl(dev, src, reg_b); break;
        case CPU_MNEM_SHR:  cpu_shr(dev, src, reg_b); break;
        case CPU_MNEM_SAR:  cpu_sar(dev, src, reg_b); break;
        case CPU_MNEM_MUL:  cpu_mul(dev, src, reg_b); break;
        case CPU_MNEM_DOT4: cpu_dot4(dev, reg_a, reg_b, (uint8_t) imm); break;
        case CPU_MNEM_POPCNT: cpu_popcnt(dev, src, reg_b); break;

        case CPU_MNEM_JMP:  cpu_jmp(dev, target); break;
        case CPU_MNEM_CALL: cpu_call(dev, target); break;
        case CPU_MNEM_RET:  cpu_ret(dev); break;

        case CPU_MNEM_MFA:   cpu_mfa(dev, reg_a, reg_b); break;
        case CPU_MNEM_MTA:   cpu_mta(dev, src, reg_b); break;
        case CPU_MNEM_DOTV:  cpu_dotv(dev, reg_b); break;
        case CPU_MNEM_DOTM:  cpu_dotm(dev, reg_b); break;
        case CPU_MNEM_LEARN: cpu_learn(dev, src); break;
        case CPU_MNEM_XSH:   cpu_xsh(dev, src); break;
        case CPU_MNEM_SSAT:  cpu_ssat(dev, reg_b); break;
        case CPU_MNEM_USAT:  cpu_usat(dev, reg_b); break;

        default: {
            if ((op & 0xf0) == CPU_MNEM_JCC) {
                cpu_jcc(dev, op & 0x0f, target);
            } else if ((op & 0xf0) == CPU_MNEM_SETCC) {
                cpu_setcc(dev, op & 0x0f, reg_b);
            } else {
                assert(false); // `cpu_decode` accepts no other opcode
            }
        }
    }

    if (!cpu->trapped) {
        cpu->ip = cpu->next_ip;
        cpu->retired = true;
    }
}

static void mem_step(device_t *dev)
{
    assert(dev != NULL);

    // empty for now, as we don't need to refresh DRAM in our emulator :)
}

static void pic_step(device_t *dev)
{
    assert(dev != NULL);

    // empty for now
}

static void pit_step(device_t *dev)
{
    assert(dev != NULL);

    uint16_t cnt = dev->pit.cnt_high << 8 | dev->pit.cnt_low;
    uint16_t freq_div = dev->pit.freq_div_high << 8 | dev->pit.freq_div_low;

    if (cnt == 1) {
        cnt = freq_div;
        cpu_raise_irq(dev, PIT_IRQ);
    } else {
        --cnt;
    }

    dev->pit.cnt_low  = (uint8_t) (cnt & 0xff);
    dev->pit.cnt_high = (uint8_t) ((cnt >> 8) & 0xff);
}

static void kbdc_step(device_t *dev, bool has_external_scancode, uint8_t external_scancode)
{
    assert(dev != NULL);

    if (has_external_scancode) {
        if (dev->kbdc.status & KBDC_FLAG_WAITING) { // the previous byte was not read and is lost
            dev->kbdc.status |= KBDC_FLAG_OVERRUN;
        }
        dev->kbdc.scancode = external_scancode;
        dev->kbdc.status |= KBDC_FLAG_WAITING;

        cpu_raise_irq(dev, KBDC_IRQ);
    }
}

static void rp_step(device_t *dev)
{
    assert(dev != NULL);

    rp_t *rp = &dev->rp;

    rp->time_us += RP_US_PER_STEP;

    if ((rp->disk_stat & RP_DISK_FLAG_BUSY) && --rp->disk_steps == 0) {
        // The RP2040 copies a whole sector between the disk and RAM by itself; the CPU only starts the command
        bool is_valid = rp->disk != NULL &&
                        (rp->disk_cmd == RP_DISK_CMD_READ || rp->disk_cmd == RP_DISK_CMD_WRITE) &&
                        (uint64_t) rp->disk_sec * RP_DISK_SEC_SIZE + RP_DISK_SEC_SIZE <= rp->disk_size &&
                        rp->disk_buf % 4 == 0 &&
                        rp->disk_buf >= RAM_BASE && (uint64_t) rp->disk_buf + RP_DISK_SEC_SIZE <= RAM_BASE + RAM_SIZE;
        if (is_valid) {
            uint8_t *sec = rp->disk + (size_t) rp->disk_sec * RP_DISK_SEC_SIZE;
            uint8_t *buf = dev->mem.ram + (rp->disk_buf - RAM_BASE);
            if (rp->disk_cmd == RP_DISK_CMD_READ) {
                memcpy(buf, sec, RP_DISK_SEC_SIZE);
            } else {
                memcpy(sec, buf, RP_DISK_SEC_SIZE);
            }
        }

        rp->disk_cmd = 0;
        rp->disk_stat = is_valid ? 0 : RP_DISK_FLAG_ERROR;
        rp->irq_stat |= RP_IRQ_FLAG_DISK;
    }

    if ((rp->scr_stat & RP_SCR_FLAG_BUSY) && --rp->scr_steps == 0) {
        memcpy(rp->scr_shown, rp->scr_sent, RP_SCR_SIZE);
        ++rp->scr_frames;
        rp->scr_stat = 0;
    }

    // The console shows what it has drawn by itself; the emulated screen takes it at once
    if (rp->scr_console && rp->scr_dirty) {
        memcpy(rp->scr_shown, rp->scr, RP_SCR_SIZE);
        rp->scr_dirty = false;
    }
}

static void dev_step(device_t *dev, bool has_external_scancode, uint8_t external_scancode)
{
    assert(dev != NULL);

    cpu_step(dev);
    mem_step(dev);
    pic_step(dev);
    pit_step(dev);
    kbdc_step(dev, has_external_scancode, external_scancode);
    rp_step(dev);
    pic_deliver_irq(dev); // pending requests are delivered at the instruction boundary
}

/* Helper Functions */

static bool cpu_is_parked(cpu_t *cpu)
{
    assert(cpu != NULL);

    // A halted CPU that cannot take an interrupt will never run again
    return cpu->halted && (!(cpu->flags & CPU_FLAG_I) || (cpu->ctrl & CPU_CTRL_T));
}

static void firmware_free(firmware_t *firmware)
{
    assert(firmware != NULL);

    if (firmware->image != NULL) {
        free(firmware->image);
    }

    firmware->image = NULL;
    firmware->image_size = 0;

    firmware->data_sec = NULL;
    firmware->data_sec_size = 0;

    firmware->text_sec = NULL;
    firmware->text_sec_size = 0;
}

static bool firmware_load(const char *path, firmware_t *firmware)
{
    assert(path != NULL);
    assert(firmware != NULL);

    bool status = false;

    firmware->image = NULL;

    FILE *in_file = fopen(path, "rb");
    if (in_file == NULL) {
        perror("Failed to open the compiled binary file");
        goto end;
    }

    bin_fmt_header_t bin_fmt_header;
    if (fread(&bin_fmt_header, sizeof(bin_fmt_header), 1, in_file) != 1) {
        fputs("Failed to read the header of the compiled binary file\n", stderr);
        goto end;
    }

    if (bin_fmt_header.magic != BIN_FMT_MAGIC) {
        fputs("The compiled binary file has an incorrect magic number\n", stderr);
        goto end;
    }

    if (bin_fmt_header.text_sec_size > MAX_FIRMWARE_TEXT_SIZE) {
        fputs("The size of the text section of the compiled binary is too big\n", stderr);
        goto end;
    }

    if (bin_fmt_header.data_sec_size > MAX_FIRMWARE_DATA_SIZE) {
        fputs("The size of the data section of the compiled binary is too big\n", stderr);
        goto end;
    }

    firmware->image_size = (size_t) bin_fmt_header.text_sec_size + bin_fmt_header.data_sec_size;
    firmware->image = (uint8_t *) malloc(firmware->image_size > 0 ? firmware->image_size : 1);
    if (firmware->image == NULL) {
        perror("Not enough memory to load the text and data sections from the compiled binary file");
        goto end;
    }

    firmware->text_sec = firmware->image;
    firmware->text_sec_size = bin_fmt_header.text_sec_size;

    firmware->data_sec = firmware->image + firmware->text_sec_size;
    firmware->data_sec_size = bin_fmt_header.data_sec_size;

    if (firmware->image_size > 0 && fread(firmware->image, firmware->image_size, 1, in_file) != 1) {
        fputs("The size of the data and text sections specified in the file is bigger than the file itself\n", stderr);
        goto end;
    }

    status = true;

end:
    if (!status) {
        firmware_free(firmware);
    }

    if (in_file != NULL) {
        fclose(in_file);
    }

    return status;
}

static void cpu_to_string(cpu_t *cpu, char *buff, size_t buff_size)
{
    assert(cpu != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    size_t pos = 0;

    int res = snprintf(
        buff, buff_size,

        "CPU State:\n"
        " General-Purpose Registers:\n"
        "  r0: 0x%08" PRIx32 "  r4: 0x%08" PRIx32 "\n"
        "  r1: 0x%08" PRIx32 "  r5: 0x%08" PRIx32 "\n"
        "  r2: 0x%08" PRIx32 "  r6: 0x%08" PRIx32 "\n"
        "  r3: 0x%08" PRIx32 "  r7: 0x%08" PRIx32 "\n"
        " Special-Purpose Registers:\n"
        "  fp: 0x%08" PRIx32 "  lr: 0x%08" PRIx32 "\n"
        "  sp: 0x%08" PRIx32 " (kernel mode)\n"
        "  sp: 0x%08" PRIx32 " (user mode)\n"
        "  ip: 0x%08" PRIx32 "%s\n"
        "  flags: %u%u%u%u%u%u, 0x%02" PRIx32 "\n"
        "         UIOSZC\n"
        " Control Registers:\n"
        "  cr0: %u%u%u%u (control)\n"
        "       TFSP\n"
        "  cr1: 0x%08" PRIx32 " (saved ip)\n"
        "  cr2: 0x%08" PRIx32 " (fault address)\n"
        "  cr3: 0x%08" PRIx32 " (top page table)\n"
        "  cr4: 0x%08" PRIx32 " (saved flags)\n"
        "  cr5: 0x%08" PRIx32 " (cause)\n"
        " Translation Lookaside Buffer:",

        cpu->r0, cpu->r4,
        cpu->r1, cpu->r5,
        cpu->r2, cpu->r6,
        cpu->r3, cpu->r7,

        cpu->fp, cpu->lr,
        cpu->ksp,
        cpu->usp,
        cpu->ip, cpu->double_fault ? " (double fault)" : (cpu->halted ? " (halted)" : ""),
        (unsigned) ((cpu->flags & CPU_FLAG_U) != 0),
        (unsigned) ((cpu->flags & CPU_FLAG_I) != 0),
        (unsigned) ((cpu->flags & CPU_FLAG_O) != 0),
        (unsigned) ((cpu->flags & CPU_FLAG_S) != 0),
        (unsigned) ((cpu->flags & CPU_FLAG_Z) != 0),
        (unsigned) ((cpu->flags & CPU_FLAG_C) != 0),
        cpu->flags,

        (unsigned) ((cpu->ctrl & CPU_CTRL_T) != 0),
        (unsigned) ((cpu->ctrl & CPU_CTRL_F) != 0),
        (unsigned) ((cpu->ctrl & CPU_CTRL_S) != 0),
        (unsigned) ((cpu->ctrl & CPU_CTRL_P) != 0),
        cpu->saved_ip,
        cpu->fault_addr,
        cpu->pt_root,
        cpu->saved_flags,
        cpu->cause
    );
    if (res < 0 || (ssize_t) buff_size - (ssize_t) (pos += res) <= 1) {
        goto out;
    }

    for (size_t i = 0; i < CPU_TLB_SIZE; ++i) {
        tlb_entry_t *entry = &cpu->tlb[i];
        if (entry->valid) {
            res = snprintf(
                buff + pos,
                buff_size - pos,
                "\n  %c%zu: 0x%08" PRIx32 " -> 0x%08" PRIx32 " %c%c",
                i == cpu->tlb_next ? '>' : ' ', i,
                entry->vpn << 12, entry->pfn << 12,
                entry->user ? 'U' : '-', entry->writable ? 'W' : '-'
            );
        } else {
            res = snprintf(
                buff + pos,
                buff_size - pos,
                "\n  %c%zu: empty",
                i == cpu->tlb_next ? '>' : ' ', i
            );
        }
        if (res < 0 || (ssize_t) buff_size - (ssize_t) (pos += res) <= 1) {
            break;
        }
    }

    if (pos < buff_size) {
        res = snprintf(buff + pos, buff_size - pos, "\n Internal Data:\n  cycles: %zu", cpu->cycles);
        if (res > 0) {
            pos += res;
        }
    }

out:
    if (pos < buff_size) {
        buff[pos] = '\0';
    } else {
        buff[buff_size - 1] = '\0';
    }
}

static void mem_to_string(mem_t *mem, uint32_t addr, size_t rows, char *buff, size_t buff_size)
{
    assert(mem != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    size_t pos = 0;
    for (size_t i = 0; i < rows * 8; ++i) {
        uint32_t byte_addr = addr + (uint32_t) i;
        if (i % 8 == 0) {
            int res = snprintf(
                buff + pos,
                buff_size - pos,
                i == 0 ? "Memory State:\n [0x%08" PRIx32 "]:" : "\n [0x%08" PRIx32 "]:",
                byte_addr
            );
            if (res < 0 || (ssize_t) buff_size - (ssize_t) (pos += res) <= 1) {
                break;
            }
        }

        int res;
        if (byte_addr >= RAM_BASE && byte_addr < RAM_BASE + RAM_SIZE) {
            res = snprintf(buff + pos, buff_size - pos, " 0x%02" PRIx8, mem->ram[byte_addr - RAM_BASE]);
        } else if (byte_addr >= ROM_BASE && byte_addr < ROM_BASE + ROM_SIZE) {
            res = snprintf(buff + pos, buff_size - pos, " 0x%02" PRIx8, mem->rom[byte_addr - ROM_BASE]);
        } else {
            res = snprintf(buff + pos, buff_size - pos, " ----"); // nothing answers at this physical address
        }
        if (res < 0 || (ssize_t) buff_size - (ssize_t) (pos += res) <= 1) {
            break;
        }
    }

    if (pos < buff_size) {
        buff[pos] = '\0';
    } else {
        buff[buff_size - 1] = '\0';
    }
}

static void pic_to_string(pic_t *pic, char *buff, size_t buff_size)
{
    assert(pic != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    snprintf(
        buff, buff_size,

        "Programmable Interrupt Controller's State:\n"
        " imr: %u%u%u%u%u%u%u%u, 0x%02" PRIx8 " (%" PRIu8 ")\n"
        " irr: %u%u%u%u%u%u%u%u, 0x%02" PRIx8 " (%" PRIu8 ")\n"
        " vec_off: 0x%02" PRIx8,

        (pic->imr & 0b10000000) >> 7,
        (pic->imr & 0b01000000) >> 6,
        (pic->imr & 0b00100000) >> 5,
        (pic->imr & 0b00010000) >> 4,
        (pic->imr & 0b00001000) >> 3,
        (pic->imr & 0b00000100) >> 2,
        (pic->imr & 0b00000010) >> 1,
        (pic->imr & 0b00000001),
        pic->imr, pic->imr,
        (pic->irr & 0b10000000) >> 7,
        (pic->irr & 0b01000000) >> 6,
        (pic->irr & 0b00100000) >> 5,
        (pic->irr & 0b00010000) >> 4,
        (pic->irr & 0b00001000) >> 3,
        (pic->irr & 0b00000100) >> 2,
        (pic->irr & 0b00000010) >> 1,
        (pic->irr & 0b00000001),
        pic->irr, pic->irr,
        pic->vec_off
    );
}

static void pit_to_string(pit_t *pit, char *buff, size_t buff_size)
{
    assert(pit != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    snprintf(
        buff, buff_size,

        "Programmable Interval Timer's State:\n"
        " freq_div: 0x%02" PRIx8 ":0x%02" PRIx8 " (%" PRIu16 ")\n"
        " cnt:      0x%02" PRIx8 ":0x%02" PRIx8 " (%" PRIu16 ")",

        pit->freq_div_high, pit->freq_div_low,
        (uint16_t) (pit->freq_div_high << 8 | pit->freq_div_low),

        pit->cnt_high, pit->cnt_low,
        (uint16_t) (pit->cnt_high << 8 | pit->cnt_low)
    );
}

static void kbdc_to_string(kbdc_t *kbdc, char *buff, size_t buff_size)
{
    assert(kbdc != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    snprintf(
        buff, buff_size,

        "Keyboard Controller's State:\n"
        " scancode: 0x%02" PRIx8 "\n"
        " status: %u%u\n"
        "         OW",

        kbdc->scancode,
        (unsigned) ((kbdc->status & KBDC_FLAG_OVERRUN) != 0),
        (unsigned) ((kbdc->status & KBDC_FLAG_WAITING) != 0)
    );
}

static void rp_to_string(rp_t *rp, char *buff, size_t buff_size)
{
    assert(rp != NULL);

    assert(buff != NULL);
    assert(buff_size > 0);

    uint32_t irq_stat = rp_get_irq_stat(rp);

    snprintf(
        buff, buff_size,

        "RP2040's Devices:\n"
        " time: %" PRIu32 " us\n"
        " console input: %zu byte(s) waiting\n"
        " irq_stat:   %u%u\n"
        " irq_enable: %u%u\n"
        "             DC\n"
        " disk: sector %" PRIu32 ", buffer 0x%08" PRIx32 "\n"
        " disk_stat: %u%u\n"
        "            EB",

        rp->time_us,
        rp->con_in_cnt,
        (unsigned) ((irq_stat & RP_IRQ_FLAG_DISK) != 0),
        (unsigned) ((irq_stat & RP_IRQ_FLAG_CON) != 0),
        (unsigned) ((rp->irq_enable & RP_IRQ_FLAG_DISK) != 0),
        (unsigned) ((rp->irq_enable & RP_IRQ_FLAG_CON) != 0),
        rp->disk_sec, rp->disk_buf,
        (unsigned) ((rp->disk_stat & RP_DISK_FLAG_ERROR) != 0),
        (unsigned) ((rp->disk_stat & RP_DISK_FLAG_BUSY) != 0)
    );
}

#endif /* ASC_H */
