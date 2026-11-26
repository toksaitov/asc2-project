/*
 *  Almurut II code generator for TCC
 *
 *  Modelled on i386-gen.c, Copyright (c) 2001-2004 Fabrice Bellard
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

/*
    TCC compiles an expression with a stack of values. A value is a
    constant, a place in memory, a register, or the result of a comparison
    that is still in the flags. The front end asks this file for small
    steps: load a value into a register, store a register, add the two
    values on top of the stack, call a function, jump.

    The registers of Almurut II as this file uses them:

        r0, r1, r2  hold the values of an expression. TCC picks them.
        r3          is this file's own: it holds an address for a moment
                    when a store needs one.
        r4-r7       are never used, so a function keeps them for its
                    caller with no code.
        fp, sp, lr  the frame pointer, the stack pointer, and the return
                    address that `call` leaves.

    The calling convention:

        * The caller pushes the arguments, the last one first, and removes
          them after the call. Each argument takes a multiple of 4 bytes.
          A 64-bit argument is two words, the low one at the lower address.
          A structure is copied onto the stack.
        * The result comes back in r0, and a 64-bit result in r0 (low) and
          r1 (high). A result narrower than 32 bits fills all of r0 with
          its sign or with zeros. For a structure the caller passes the
          address of a place for it as a hidden first argument.
        * A function may change r0-r3 and the flags. It restores r4-r7,
          fp, and sp.

    A function starts and ends with

        push lr             mov fp sp
        push fp             pop fp
        mov sp fp           pop lr
        sub SIZE sp         ret

    so that inside it 0(fp) is the caller's fp, 4(fp) the return address,
    8(fp) the first argument, and the local variables are below fp.

    There is no floating point: the machine has none.
*/

#ifdef TARGET_DEFS_ONLY

/* number of available registers */
#define NB_REGS 4

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT   0x0001 /* generic integer register */
#define RC_FLOAT 0x0002 /* generic float register */
#define RC_R0    0x0004
#define RC_R1    0x0008
#define RC_R2    0x0010
#define RC_F0    0x0020

#define RC_IRET RC_R0 /* function return: integer register */
#define RC_LRET RC_R1 /* function return: second integer register */
#define RC_FRET RC_F0 /* function return: float register */

/* pretty names for the registers */
enum {
    TREG_R0 = 0,
    TREG_R1,
    TREG_R2,
    TREG_F0 /* stands for a float register so that TCC's tables are complete; no code uses it */
};

/* return registers for function */
#define REG_IRET TREG_R0 /* single word int return register */
#define REG_LRET TREG_R1 /* second word return register (for long long) */
#define REG_FRET TREG_F0 /* float return register */

/* pointer size, in bytes */
#define PTR_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  8
#define LDOUBLE_ALIGN 8
/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8

/******************************************************/
#else /* ! TARGET_DEFS_ONLY */
/******************************************************/
#include "tcc.h"

ST_DATA const int reg_classes[NB_REGS] = {
    /* r0 */ RC_INT | RC_R0,
    /* r1 */ RC_INT | RC_R1,
    /* r2 */ RC_INT | RC_R2,
    /* f0 */ RC_FLOAT | RC_F0,
};

/* Registers that TCC does not pick, by their numbers in an instruction */
enum {
    REG_R3 = 3,
    REG_FP = 8,
    REG_SP = 9,
    REG_LR = 10
};

/* Opcodes: the numbers of `cpu_mnem_code_t` in asc2-emulated/asc.h */
enum {
    OPC_NOP   = 0x01,
    OPC_MOV   = 0x10, /* with OPC_IMM: `li` */
    OPC_LIH   = 0x11,
    OPC_PUSH  = 0x12,
    OPC_POP   = 0x13,
    OPC_SR    = 0x14,
    OPC_SRH   = 0x15,
    OPC_SRB   = 0x16,
    OPC_LR    = 0x18,
    OPC_LRH   = 0x19,
    OPC_LRB   = 0x1a,
    OPC_LRHU  = 0x1d,
    OPC_LRBU  = 0x1e,
    OPC_ADD   = 0x20,
    OPC_ADC   = 0x21,
    OPC_SUB   = 0x22,
    OPC_SBB   = 0x23,
    OPC_CMP   = 0x24,
    OPC_AND   = 0x25,
    OPC_OR    = 0x27,
    OPC_XOR   = 0x28,
    OPC_SHL   = 0x2a,
    OPC_SHR   = 0x2b,
    OPC_SAR   = 0x2c,
    OPC_MUL   = 0x2d,
    OPC_JMP   = 0x30,
    OPC_CALL  = 0x31,
    OPC_RET   = 0x32,
    OPC_JCC   = 0x40, /* plus a condition */
    OPC_SETCC = 0x50, /* plus a condition */
    OPC_IMM   = 0x80  /* the first operand is the immediate value */
};

/* Conditions: the numbers of `cpu_cond_code_t` in asc2-emulated/asc.h.
   A condition and its opposite differ in the lowest bit. */
enum {
    COND_E, COND_NE, COND_L, COND_GE, COND_LE, COND_G, COND_B, COND_AE, COND_BE, COND_A
};

static unsigned long func_sub_sp_offset;

/******************************************************/
/* Instruction words */

/* An instruction is one 32-bit word:

       bits  7:0   opcode
       bits 11:8   register B, the second operand in assembly
       bits 15:12  register A, the first operand in assembly
       bits 31:16  16-bit immediate value */

/* output an instruction word */
ST_FUNC void o(unsigned int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 4;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    write32le(cur_text_section->data + ind, c);
    ind = ind1;
}

static int fits_imm(int c)
{
    return c == (short)c;
}

/* `opc ra rb`, two registers: `add r1 r0` */
static void gen_rr(int opc, int ra, int rb)
{
    o(opc | (rb << 8) | (ra << 12));
}

/* `opc imm rb`, an immediate value and a register: `add 0x4 r0` */
static void gen_ir(int opc, int imm, int rb)
{
    o(opc | OPC_IMM | (rb << 8) | ((unsigned)(imm & 0xffff) << 16));
}

/* `opc disp(ra) rb`, a load, or `opc ra disp(rb)`, a store */
static void gen_mem(int opc, int ra, int rb, int disp)
{
    o(opc | (rb << 8) | (ra << 12) | ((unsigned)(disp & 0xffff) << 16));
}

/* `opc imm`, an immediate value alone: `push 0x10`, or a jump or a call to
   the instruction `imm` instructions away */
static void gen_i(int opc, int imm)
{
    o(opc | OPC_IMM | ((unsigned)(imm & 0xffff) << 16));
}

/* `li c r`. A constant that does not fit into 16 bits with a sign, or the
   address of a symbol, which only the linker knows, takes a second
   instruction: `lih` replaces the upper half. Neither changes the flags. */
static void gen_li(int r, Sym *sym, int c)
{
    if (sym) {
        greloca(cur_text_section, sym, ind, R_ASC2_LO16, c);
        gen_ir(OPC_MOV, 0, r);
        greloca(cur_text_section, sym, ind, R_ASC2_HI16, c);
        gen_ir(OPC_LIH, 0, r);
    } else {
        gen_ir(OPC_MOV, c, r);
        if (!fits_imm(c))
            gen_ir(OPC_LIH, c >> 16, r);
    }
}

/* `add c r`, to move the stack pointer or to find the address of a local
   variable. The 16-bit field limits the local variables of a function and
   the arguments of a call to 32 KB each. */
static void gen_add_const(int c, int r)
{
    if (!fits_imm(c))
        tcc_error("stack frame too large: more than 32 KB of local variables or arguments");
    gen_ir(OPC_ADD, c, r);
}

/******************************************************/
/* Jumps */

/* A jump whose target is not known yet waits in a list. The list lives in
   the jumps themselves: the immediate field of each one holds the distance,
   in instructions, back to the jump before it in the list, and zero in the
   last one. TCC names a list by the position of its first jump, and zero
   is the empty list. When the target is known, gsym_addr() walks the list
   and writes the real distances. */

static int jmp_dist(int from, int to)
{
    int d = (to - from) / 4;
    if (!fits_imm(d))
        tcc_error("function too large: a jump does not reach its target");
    return d;
}

static int list_next(int pos)
{
    int d = (short)(read32le(cur_text_section->data + pos) >> 16);
    return d ? pos - d * 4 : 0;
}

static void set_imm(int pos, int imm)
{
    unsigned char *p = cur_text_section->data + pos;
    write32le(p, (read32le(p) & 0xffff) | ((unsigned)(imm & 0xffff) << 16));
}

/* output the jump `opc` and put it in front of the list t */
static int gjmp_list(int opc, int t)
{
    int r = ind;
    if (nocode_wanted)
        return t;
    gen_i(opc, t ? jmp_dist(t, r) : 0);
    return r;
}

/* the target of all jumps of the list t is the address a */
ST_FUNC void gsym_addr(int t, int a)
{
    int next;
    while (t) {
        next = list_next(t);
        set_imm(t, jmp_dist(t, a));
        t = next;
    }
}

ST_FUNC void gsym(int t)
{
    gsym_addr(t, ind);
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t)
{
    return gjmp_list(OPC_JMP, t);
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a)
{
    gen_i(OPC_JMP, jmp_dist(ind, a));
}

/* the condition of a comparison operator */
static int cond_of(int op)
{
    switch (op) {
    case TOK_EQ:  return COND_E;
    case TOK_NE:  return COND_NE;
    case TOK_LT:  return COND_L;
    case TOK_GE:  return COND_GE;
    case TOK_LE:  return COND_LE;
    case TOK_GT:  return COND_G;
    case TOK_ULT: return COND_B;
    case TOK_UGE: return COND_AE;
    case TOK_ULE: return COND_BE;
    case TOK_UGT: return COND_A;
    }
    tcc_error("unknown comparison");
    return 0;
}

/* generate a test. set 'inv' to invert test. Stack entry is popped */
ST_FUNC int gtst(int inv, int t)
{
    int v = vtop->r & VT_VALMASK;
    if (nocode_wanted) {
        ;
    } else if (v == VT_CMP) {
        /* the flags are set: jump on them */
        t = gjmp_list(OPC_JCC + (cond_of(vtop->c.i) ^ inv), t);
    } else if (v == VT_JMP || v == VT_JMPI) {
        /* && or || optimization */
        if ((v & 1) == inv) {
            /* put the list of the value in front of t */
            int n = vtop->c.i, n1;
            if (n) {
                while ((n1 = list_next(n)))
                    n = n1;
                if (t)
                    set_imm(n, jmp_dist(t, n));
                t = vtop->c.i;
            }
        } else {
            t = gjmp(t);
            gsym(vtop->c.i);
        }
    }
    vtop--;
    return t;
}

/******************************************************/
/* Loads and stores */

/* The base register and the displacement of a place in memory. A local
   variable is at `c(fp)`. The address of a global one goes to the register
   `tmp` first. Any other place has its address in a register already. */
static int gen_base(SValue *sv, int tmp, int *disp)
{
    int v = sv->r & VT_VALMASK;
    int c = sv->c.i;

    *disp = 0;
    if (v == VT_LOCAL) {
        if (!fits_imm(c))
            tcc_error("stack frame too large: a local variable is more than 32 KB away from fp");
        *disp = c;
        return REG_FP;
    } else if (v == VT_CONST) {
        gen_li(tmp, (sv->r & VT_SYM) ? sv->sym : NULL, c);
        return tmp;
    }
    return v;
}

/* load 'r' from value 'sv' */
ST_FUNC void load(int r, SValue *sv)
{
    int v, t, ft, fc, fr, bt, base, disp, opc;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t & ~(VT_DEFSIGN | VT_VOLATILE | VT_CONSTANT);
    fc = sv->c.i;
    bt = ft & VT_BTYPE;

    if (is_float(ft))
        tcc_error("floating point is not supported");

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        if (v == VT_LLOCAL) {
            /* the address is in a local variable: load it first */
            v1.type.t = VT_INT;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.i = fc;
            load(r, &v1);
            base = r;
            disp = 0;
        } else {
            base = gen_base(sv, r, &disp);
        }
        if (bt == VT_BYTE || bt == VT_BOOL)
            opc = (ft & VT_UNSIGNED) ? OPC_LRBU : OPC_LRB;
        else if (bt == VT_SHORT)
            opc = (ft & VT_UNSIGNED) ? OPC_LRHU : OPC_LRH;
        else
            opc = OPC_LR;
        gen_mem(opc, base, r, disp);
    } else {
        if (v == VT_CONST) {
            gen_li(r, (fr & VT_SYM) ? sv->sym : NULL, fc);
        } else if (v == VT_LOCAL) {
            /* the address of a local variable */
            gen_rr(OPC_MOV, REG_FP, r);
            if (fc)
                gen_add_const(fc, r);
        } else if (v == VT_CMP) {
            /* 1 or 0 from the flags: `setcc r` */
            o((OPC_SETCC + cond_of(fc)) | (r << 8));
        } else if (v == VT_JMP || v == VT_JMPI) {
            /* 1 or 0 from a list of jumps: the jumps arrive at the
               second `li`, and the code that falls through skips it */
            t = v & 1;
            gen_ir(OPC_MOV, t, r);
            gen_i(OPC_JMP, 2);
            gsym(fc);
            gen_ir(OPC_MOV, t ^ 1, r);
        } else if (v != r) {
            gen_rr(OPC_MOV, v, r);
        }
    }
}

/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *v)
{
    int fr, bt, ft, base, disp, opc;

    ft = v->type.t & ~(VT_VOLATILE | VT_CONSTANT);
    fr = v->r & VT_VALMASK;
    bt = ft & VT_BTYPE;

    if (is_float(ft))
        tcc_error("floating point is not supported");

    /* TCC stores into memory only: it keeps no variable in a register */
    if (!(fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)))
        tcc_error("internal error: a store into a register");

    base = gen_base(v, REG_R3, &disp);
    if (bt == VT_BYTE || bt == VT_BOOL)
        opc = OPC_SRB;
    else if (bt == VT_SHORT)
        opc = OPC_SRH;
    else
        opc = OPC_SR;
    gen_mem(opc, r, base, disp);
}

/******************************************************/
/* Function calls */

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp)
{
    int r, opc;

    opc = is_jmp ? OPC_JMP : OPC_CALL;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) {
        /* a function known by its name: the linker writes the distance */
        greloca(cur_text_section, vtop->sym, ind, R_ASC2_REL16, vtop->c.i);
        gen_i(opc, 0);
    } else {
        /* otherwise, indirect call */
        r = gv(RC_INT);
        gen_rr(opc, r, 0);
    }
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize)
{
    *ret_align = 1;
    return 0;
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
ST_FUNC void gfunc_call(int nb_args)
{
    int size, align, r, args_size, i;

    args_size = 0;
    for (i = 0; i < nb_args; i++) {
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            size = type_size(&vtop->type, &align);
            /* align to stack align size */
            size = (size + 3) & ~3;
            /* allocate the necessary size on stack */
            gen_add_const(-size, REG_SP);
            /* generate structure store */
            r = get_reg(RC_INT);
            gen_rr(OPC_MOV, REG_SP, r);
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
        } else if (is_float(vtop->type.t)) {
            tcc_error("floating point is not supported");
            size = 0;
        } else if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
            /* the high word first, so that the low one is below it */
            size = 8;
            r = gv(RC_INT);
            gen_rr(OPC_PUSH, vtop->r2, 0);
            gen_rr(OPC_PUSH, r, 0);
        } else if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && fits_imm(vtop->c.i)) {
            size = 4;
            gen_i(OPC_PUSH, vtop->c.i);
        } else {
            size = 4;
            r = gv(RC_INT);
            gen_rr(OPC_PUSH, r, 0);
        }
        args_size += size;
        vtop--;
    }
    save_regs(0); /* save used temporary registers */
    gcall_or_jmp(0);
    if (args_size)
        gen_add_const(args_size, REG_SP);
    vtop--;
}

#define FUNC_PROLOG_SIZE 16

/* generate function prolog of type 't' */
ST_FUNC void gfunc_prolog(CType *func_type)
{
    int addr, align, size;
    Sym *sym;
    CType *type;

    sym = func_type->ref;
    addr = 8;
    loc = 0;
    func_vc = 0;

    /* the size of the local variables is known at the end of the
       function only: leave room and let gfunc_epilog() fill it */
    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;
    /* if the function returns a structure, then add an
       implicit pointer parameter */
    func_vt = sym->type;
    func_var = (sym->f.func_type == FUNC_ELLIPSIS);
    if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
        func_vc = addr;
        addr += 4;
    }
    /* define parameters */
    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        size = type_size(type, &align);
        size = (size + 3) & ~3;
        sym_push(sym->v & ~SYM_FIELD, type,
                 VT_LOCAL | lvalue_type(type->t), addr);
        addr += size;
    }
}

/* generate function epilog */
ST_FUNC void gfunc_epilog(void)
{
    int v, saved_ind;

    gen_rr(OPC_MOV, REG_FP, REG_SP);
    o(OPC_POP | (REG_FP << 8));
    o(OPC_POP | (REG_LR << 8));
    o(OPC_RET);

    /* align local size to word & save local variables */
    v = (-loc + 3) & -4;
    if (!fits_imm(v))
        tcc_error("stack frame too large: the local variables take more than 32 KB");
    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
    gen_rr(OPC_PUSH, REG_LR, 0);
    gen_rr(OPC_PUSH, REG_FP, 0);
    gen_rr(OPC_MOV, REG_SP, REG_FP);
    if (v)
        gen_ir(OPC_SUB, v, REG_SP);
    else
        o(OPC_NOP);
    ind = saved_ind;
}

/******************************************************/
/* Arithmetic */

/* a call to a function of the runtime library with the two values on top
   of the stack as its arguments; the result takes their place */
static void gen_lib_call(int func, int is_llong)
{
    vpush_global_sym(&func_old_type, func);
    vrott(3);
    gfunc_call(2);
    vpushi(0);
    vtop->r = REG_IRET;
    if (is_llong)
        vtop->r2 = REG_LRET;
}

/* generate an integer binary operation */
ST_FUNC void gen_opi(int op)
{
    int r, fr, opc;

    switch (op) {
    case '+':
    case TOK_ADDC1: /* add with carry generation */
        opc = OPC_ADD;
        break;
    case '-':
    case TOK_SUBC1: /* sub with carry generation */
        opc = OPC_SUB;
        break;
    case TOK_ADDC2: /* add with carry use */
        opc = OPC_ADC;
        break;
    case TOK_SUBC2: /* sub with carry use */
        opc = OPC_SBB;
        break;
    case '&':
        opc = OPC_AND;
        break;
    case '^':
        opc = OPC_XOR;
        break;
    case '|':
        opc = OPC_OR;
        break;
    case '*':
        opc = OPC_MUL;
        break;
    case TOK_SHL:
        opc = OPC_SHL;
        break;
    case TOK_SHR:
        opc = OPC_SHR;
        break;
    case TOK_SAR:
        opc = OPC_SAR;
        break;
    /* The machine has no divide instruction, and its `mul` gives the low
       32 bits of a product only. The runtime library does the rest. */
    case '/':
    case TOK_PDIV:
        gen_lib_call(TOK___divsi3, 0);
        return;
    case TOK_UDIV:
        gen_lib_call(TOK___udivsi3, 0);
        return;
    case '%':
        gen_lib_call(TOK___modsi3, 0);
        return;
    case TOK_UMOD:
        gen_lib_call(TOK___umodsi3, 0);
        return;
    case TOK_UMULL:
        gen_lib_call(TOK___umulsidi3, 1);
        return;
    default:
        /* a comparison: `cmp` sets the flags, and the value on the stack
           says which condition of them is the result */
        opc = OPC_CMP;
        break;
    }

    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && fits_imm(vtop->c.i)) {
        /* constant case */
        vswap();
        r = gv(RC_INT);
        vswap();
        gen_ir(opc, vtop->c.i, r);
    } else {
        gv2(RC_INT, RC_INT);
        r = vtop[-1].r;
        fr = vtop[0].r;
        gen_rr(opc, fr, r);
    }
    vtop--;
    if (op >= TOK_ULT && op <= TOK_GT) {
        vtop->r = VT_CMP;
        vtop->c.i = op;
    }
}

/* generate a floating point operation 'v = t1 op t2' instruction. */
ST_FUNC void gen_opf(int op)
{
    tcc_error("floating point is not supported");
}

/* convert integers to fp 't' type. */
ST_FUNC void gen_cvt_itof(int t)
{
    tcc_error("floating point is not supported");
}

/* convert fp to int 't' type */
ST_FUNC void gen_cvt_ftoi(int t)
{
    tcc_error("floating point is not supported");
}

/* convert from one floating point type to another */
ST_FUNC void gen_cvt_ftof(int t)
{
    tcc_error("floating point is not supported");
}

/* computed goto support */
ST_FUNC void ggoto(void)
{
    gcall_or_jmp(1);
    vtop--;
}

/* Save the stack pointer onto the stack */
ST_FUNC void gen_vla_sp_save(int addr)
{
    gen_mem(OPC_SR, REG_SP, REG_FP, addr);
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(int addr)
{
    gen_mem(OPC_LR, REG_FP, REG_SP, addr);
}

/* Subtract from the stack pointer, and push the resulting value onto the stack */
ST_FUNC void gen_vla_alloc(CType *type, int align)
{
    int r;
    r = gv(RC_INT); /* allocation size */
    gen_rr(OPC_SUB, r, REG_SP);
    gen_ir(OPC_AND, -4, REG_SP);
    vpop();
}

/* end of Almurut II code generator */
/*************************************************************/
#endif
/*************************************************************/
