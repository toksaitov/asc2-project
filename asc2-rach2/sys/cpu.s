# The instructions that C has no words for, as C functions: the IO ports, the
# control registers, `dot4`, the registers of the array of the dot products, and
# `hlt`.
#
# The caller has pushed the arguments, the first one on the top of the stack,
# and a function may change `r0`-`r3`.

.global read_port
.global write_port
.global read_cr0
.global write_cr0
.global write_cr3
.global dot4
.global save_array
.global load_array
.global wait_for_interrupt
.global k_halt

.section .text

# uint8_t read_port(uint8_t port)
read_port:
    lr 0(sp) r1
    inb r1 r0
    ret

# void write_port(uint8_t port, uint8_t value)
write_port:
    lr 0(sp) r1
    lr 4(sp) r0
    outb r0 r1
    ret

# uint32_t read_cr0(void)
read_cr0:
    mov cr0 r0
    ret

# void write_cr0(uint32_t value)
write_cr0:
    lr 0(sp) r0
    mov r0 cr0
    ret

# void write_cr3(uint32_t value)
#
# The CPU forgets every translation that it remembers, so the call is also the
# way to make a change of a page table visible.
write_cr3:
    lr 0(sp) r0
    mov r0 cr3
    ret

# int32_t dot4(uint32_t a, uint32_t b, int32_t sum)
#
# The compiler has no word for the instruction, so it is a function.
dot4:
    lr 0(sp) r0
    lr 4(sp) r1
    lr 8(sp) r2
    dot4 r0 r1 r2
    mov r2 r0
    ret

# void save_array(uint32_t *regs)
#
# Stores the registers of the array of the dot products at `regs`: the weights
# `w0`-`w2`, then the inputs `x0`-`x2`. The kernel may name them whatever the
# bit A of `cr0` says.
save_array:
    lr 0(sp) r1
    mov w0 r0
    sr r0 0(r1)
    mov w1 r0
    sr r0 4(r1)
    mov w2 r0
    sr r0 8(r1)
    mov x0 r0
    sr r0 12(r1)
    mov x1 r0
    sr r0 16(r1)
    mov x2 r0
    sr r0 20(r1)
    ret

# void load_array(const uint32_t *regs)
#
# Loads the registers of the array of the dot products from `regs`, in the
# order of `save_array`.
load_array:
    lr 0(sp) r1
    lr 0(r1) r0
    mov r0 w0
    lr 4(r1) r0
    mov r0 w1
    lr 8(r1) r0
    mov r0 w2
    lr 12(r1) r0
    mov r0 x0
    lr 16(r1) r0
    mov r0 x1
    lr 20(r1) r0
    mov r0 x2
    ret

# void wait_for_interrupt(void)
#
# The kernel runs with interrupts off. Here it has nothing to do until a
# device has something to say, so it lets interrupts in and halts the CPU. An
# interrupt wakes the CPU, its handler runs on the stack of the caller, and
# the function returns.
#
# An interrupt may arrive between `sti` and `hlt`. The CPU then halts after
# the handler and sleeps until the next interrupt, which the timer brings at
# the latest.
wait_for_interrupt:
    sti
    hlt
    cli
    ret

# void k_halt(uint32_t code)
#
# Stops the machine for good: no interrupt wakes a CPU that has halted with
# interrupts off. The code stays in `r0` for whoever looks at the machine.
k_halt:
    lr 0(sp) r0
    cli
_halt_again:
    hlt
    jmp _halt_again
