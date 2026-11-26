#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asc.h"

#define PROGRAM_NAME "asa"

#define MAX_LINE_LEN 4096
#define MAX_TOKEN_LEN 128
#define MAX_OPERANDS 3
#define MAX_SECTION_SIZE 0x100000
#define MAX_LABELS 8192
#define MAX_FIXUPS 32768

/* Sections */

typedef enum section_kind
{
    SECTION_TEXT,
    SECTION_DATA,
    SECTION_COUNT
} section_kind_t;

typedef struct section
{
    uint8_t *bytes;
    size_t size;
    size_t align; // the largest alignment that the code in the section has asked for
} section_t;

static section_t sections[SECTION_COUNT];

/* Labels */

typedef struct label_entry
{
    char label[MAX_TOKEN_LEN];
    bool defined; // false for a label that is only used here and lives in another object file
    bool global;  // visible to other object files
    section_kind_t section_kind;
    size_t sec_pos;
    uint32_t sym_idx; // index in the symbol table of an object file
} label_entry_t;

static size_t labels_count;
static label_entry_t labels[MAX_LABELS];

/*
    A fixup is a place in a section that needs the address of a label. The
    address is not known while the line is assembled: the label may be defined
    further down the file or in another object file. The kinds of fixups are
    the relocation types of an object file (`ELF_RELOC_ASC2_` constants).
*/
typedef struct fixup_entry
{
    uint32_t kind;
    char label[MAX_TOKEN_LEN];
    section_kind_t section_kind;
    size_t sec_pos;
    size_t line_num;
} fixup_entry_t;

static size_t fixups_count;
static fixup_entry_t fixups[MAX_FIXUPS];

/* Parsed Lines */

typedef enum command_type
{
    INVALID,
    BLANK,
    COMMENT,
    DIRECTIVE,
    LABEL,
    INSTRUCTION,
} command_type_t;

typedef struct command
{
    command_type_t type;
    char token[MAX_TOKEN_LEN];
    char args[MAX_OPERANDS][MAX_TOKEN_LEN];
    size_t args_count;
    const char *text; // the rest of the line after the name, for the directives that take a string
} command_t;

typedef enum operand_kind
{
    OPERAND_REG,      // a register:                    r0
    OPERAND_CTRL_REG, // a control register:            cr3
    OPERAND_DOT_REG,  // a register of the array:       w0
    OPERAND_VALUE,    // a number:                      0x10
    OPERAND_LABEL,    // a label:                       main
    OPERAND_MEM,      // memory at a register plus a displacement: 0x8(fp)
} operand_kind_t;

typedef struct operand
{
    operand_kind_t kind;
    uint8_t reg;                // register, control register, or the register of a memory operand
    uint32_t value;             // number, or the displacement of a memory operand
    char label[MAX_TOKEN_LEN];
} operand_t;

/* Sections of an object file, in the order of their headers */
typedef enum elf_sec_idx
{
    ELF_SEC_IDX_NULL,
    ELF_SEC_IDX_TEXT,
    ELF_SEC_IDX_DATA,
    ELF_SEC_IDX_RELA_TEXT,
    ELF_SEC_IDX_RELA_DATA,
    ELF_SEC_IDX_SYMTAB,
    ELF_SEC_IDX_STRTAB,
    ELF_SEC_IDX_SHSTRTAB,
    ELF_SEC_IDX_COUNT
} elf_sec_idx_t;

static const char ELF_SEC_NAMES[] = "\0.text\0.data\0.rela.text\0.rela.data\0.symtab\0.strtab\0.shstrtab";
static const uint32_t ELF_SEC_NAME_POS[ELF_SEC_IDX_COUNT] = { 0, 1, 7, 13, 24, 35, 43, 51 };

/* Labels */

static bool is_label_name(const char *label)
{
    assert(label != NULL);

    if (!*label || isdigit((unsigned char) *label)) return false;

    while (*label && (isalnum((unsigned char) *label) || *label == '_')) {
        ++label;
    }

    return *label == '\0';
}

static label_entry_t *get_label(const char *label)
{
    assert(label != NULL);

    for (size_t i = 0; i < labels_count; ++i) {
        if (strncmp(labels[i].label, label, MAX_TOKEN_LEN) == 0) {
            return &labels[i];
        }
    }

    return NULL;
}

static label_entry_t *add_label(const char *label)
{
    assert(label != NULL);

    if (labels_count == MAX_LABELS) {
        return NULL;
    }

    label_entry_t *entry = &labels[labels_count++];
    strncpy(entry->label, label, MAX_TOKEN_LEN - 1);
    entry->label[MAX_TOKEN_LEN - 1] = '\0';
    entry->defined = false;
    entry->global = false;
    entry->section_kind = SECTION_TEXT;
    entry->sec_pos = 0;
    entry->sym_idx = 0;

    return entry;
}

/* Parsing */

static char* trim(char *str)
{
    assert(str != NULL);

    while (isspace((unsigned char) *str)) {
        ++str;
    }

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char) str[len - 1])) {
        --len;
    }

    str[len] = '\0';

    return str;
}

static bool is_label(const char *line)
{
    if (!*line) return false;

    while (*line && (isalnum((unsigned char) *line) || *line == '_')) {
        ++line;
    }

    return line[0] == ':' && line[1] == '\0';
}

static void parse_line(char *line, command_t *cmd)
{
    assert(line != NULL);
    assert(cmd != NULL);

    cmd->type = INVALID;
    cmd->token[0] = '\0';
    cmd->args_count = 0;
    for (size_t i = 0; i < MAX_OPERANDS; ++i) {
        cmd->args[i][0] = '\0';
    }
    cmd->text = line;

    if (*line == '\0') {
        cmd->type = BLANK;
        return;
    }

    if (*line == '#') {
        cmd->type = COMMENT;
        return;
    }

    if (is_label(line)) {
        if (sscanf(line, "%127[^:]:", cmd->token) == 1) {
            cmd->type = LABEL;
        }
        return;
    }

    char extra[2];
    int count = sscanf(line, "%127s %127s %127s %127s %1s", cmd->token, cmd->args[0], cmd->args[1], cmd->args[2], extra);
    if (count < 1) {
        return;
    }

    cmd->text = line + strlen(cmd->token);
    while (isspace((unsigned char) *cmd->text)) {
        ++cmd->text;
    }

    // A string may hold spaces, so the directives that take one read `text` and not the arguments
    bool takes_string = strcmp(cmd->token, ".ascii") == 0 || strcmp(cmd->token, ".asciz") == 0 ||
                        strcmp(cmd->token, ".incbin") == 0;
    if (count > 1 + MAX_OPERANDS && !takes_string) { // a line has a name and at most three arguments
        return;
    }
    cmd->args_count = count > 1 + MAX_OPERANDS ? MAX_OPERANDS : (size_t) count - 1;

    if (cmd->token[0] == '.') {
        memmove(cmd->token, cmd->token + 1, strlen(cmd->token));
        cmd->type = DIRECTIVE;
    } else {
        cmd->type = INSTRUCTION;
    }
}

/*
    Parses a number: hexadecimal with the `0x` prefix, binary with the `0b`
    prefix, or decimal, with an optional minus sign. The number must fit in 32
    bits, either as an unsigned number or as a negative one.
*/
static bool parse_number(const char *token, uint32_t *value)
{
    assert(token != NULL);
    assert(value != NULL);

    bool is_negative = token[0] == '-';
    const char *digits = is_negative ? token + 1 : token;

    int base = 10;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = 16;
        digits += 2;
    } else if (digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) {
        base = 2;
        digits += 2;
    }
    if (!isxdigit((unsigned char) digits[0])) { // rejects an empty number and the signs and spaces that `strtoull` would accept
        return false;
    }

    char *end_ptr;

    errno = 0;
    unsigned long long parsed_value = strtoull(digits, &end_ptr, base);
    if (errno != 0 || *end_ptr != '\0') {
        return false;
    }

    if (is_negative) {
        if (parsed_value > 0x80000000ull) {
            return false;
        }
        *value = (uint32_t) (0 - parsed_value);
    } else {
        if (parsed_value > 0xffffffffull) {
            return false;
        }
        *value = (uint32_t) parsed_value;
    }

    return true;
}

static bool get_reg_code(const char *name, const reg_enc_t *table, size_t table_size, uint8_t *code)
{
    assert(name != NULL);
    assert(table != NULL);
    assert(code != NULL);

    for (size_t i = 0; i < table_size; ++i) {
        if (strncmp(name, table[i].name, MAX_TOKEN_LEN) == 0) {
            *code = table[i].code;
            return true;
        }
    }

    return false;
}

static bool parse_operand(char *token, operand_t *operand)
{
    assert(token != NULL);
    assert(operand != NULL);

    operand->reg = 0;
    operand->value = 0;
    operand->label[0] = '\0';

    if (get_reg_code(token, REG_ENC_TABLE, CPU_REGS, &operand->reg)) {
        operand->kind = OPERAND_REG;
        return true;
    }

    if (get_reg_code(token, CTRL_REG_ENC_TABLE, CPU_CTRL_REGS, &operand->reg)) {
        operand->kind = OPERAND_CTRL_REG;
        return true;
    }

    if (get_reg_code(token, DOT_REG_ENC_TABLE, CPU_DOT_REGS, &operand->reg) && cpu_is_dot_reg(operand->reg)) {
        operand->kind = OPERAND_DOT_REG;
        return true;
    }

    if (parse_number(token, &operand->value)) {
        operand->kind = OPERAND_VALUE;
        return true;
    }

    if (is_label_name(token)) {
        operand->kind = OPERAND_LABEL;
        strncpy(operand->label, token, MAX_TOKEN_LEN);
        return true;
    }

    // What is left is a memory operand: a register in parentheses after an optional displacement, as in `0x8(fp)`
    size_t token_len = strlen(token);
    char *paren = strchr(token, '(');
    if (paren == NULL || token_len < 2 || token[token_len - 1] != ')') {
        return false;
    }

    token[token_len - 1] = '\0';
    if (!get_reg_code(paren + 1, REG_ENC_TABLE, CPU_REGS, &operand->reg)) {
        return false;
    }

    *paren = '\0';
    if (*token != '\0' && !parse_number(token, &operand->value)) {
        return false;
    }

    operand->kind = OPERAND_MEM;
    return true;
}

/* Code Generation */

static bool fits_imm(uint32_t value)
{
    // The CPU extends the sign of a 16-bit immediate value to 32 bits
    return (int32_t) value >= INT16_MIN && (int32_t) value <= INT16_MAX;
}

static bool fits_data(uint32_t value, size_t size)
{
    if (size == 4) {
        return true;
    }

    // A narrower value is either an unsigned number or a negative one of that width
    uint32_t max_unsigned = (1u << (8 * size)) - 1;
    int32_t min_signed = -(1 << (8 * size - 1));

    return value <= max_unsigned || ((int32_t) value < 0 && (int32_t) value >= min_signed);
}

static bool emit_bytes(section_kind_t section_kind, uint32_t value, size_t size)
{
    section_t *sec = &sections[section_kind];

    if (sec->size + size > MAX_SECTION_SIZE) {
        return false;
    }

    // Values are stored with the low byte first
    for (size_t i = 0; i < size; ++i) {
        sec->bytes[sec->size++] = (uint8_t) (value >> (8 * i));
    }

    return true;
}

static bool emit_inst(section_kind_t section_kind, uint8_t opcode, uint8_t reg_a, uint8_t reg_b, uint16_t imm)
{
    uint32_t inst = (uint32_t) opcode | ((uint32_t) reg_b << 8) | ((uint32_t) reg_a << 12) | ((uint32_t) imm << 16);

    return emit_bytes(section_kind, inst, CPU_INST_SIZE);
}

static bool add_fixup(uint32_t kind, const char *label, section_kind_t section_kind, size_t line_num)
{
    assert(label != NULL);

    if (fixups_count == MAX_FIXUPS) {
        return false;
    }

    fixup_entry_t *fixup = &fixups[fixups_count++];
    fixup->kind = kind;
    strncpy(fixup->label, label, MAX_TOKEN_LEN);
    fixup->section_kind = section_kind;
    fixup->sec_pos = sections[section_kind].size; // the fixup is for the bytes emitted next
    fixup->line_num = line_num;

    return true;
}

static bool operands_match(cpu_inst_fmt_t fmt, operand_t *operands, size_t operands_count)
{
    assert(operands != NULL);

    operand_kind_t kind0 = operands[0].kind;
    operand_kind_t kind1 = operands[1].kind;
    operand_kind_t kind2 = operands[2].kind;

    switch (fmt) {
        case CPU_FMT_NONE:  return operands_count == 0;
        case CPU_FMT_A:     return operands_count == 1 && kind0 == OPERAND_REG;
        case CPU_FMT_B:     return operands_count == 1 && kind0 == OPERAND_REG;
        case CPU_FMT_AB:    return operands_count == 2 && kind0 == OPERAND_REG && kind1 == OPERAND_REG;
        case CPU_FMT_I:     return operands_count == 1 && kind0 == OPERAND_VALUE;
        case CPU_FMT_IB:    return operands_count == 2 && kind0 == OPERAND_VALUE && kind1 == OPERAND_REG;
        case CPU_FMT_REL:   return operands_count == 1 && kind0 == OPERAND_LABEL;
        case CPU_FMT_LOAD:  return operands_count == 2 && (kind0 == OPERAND_MEM || kind0 == OPERAND_REG) && kind1 == OPERAND_REG;
        case CPU_FMT_STORE: return operands_count == 2 && kind0 == OPERAND_REG && (kind1 == OPERAND_MEM || kind1 == OPERAND_REG);
        case CPU_FMT_ABC:   return operands_count == 3 && kind0 == OPERAND_REG && kind1 == OPERAND_REG && kind2 == OPERAND_REG;
        case CPU_FMT_CB:    return operands_count == 2 && kind0 == OPERAND_CTRL_REG && kind1 == OPERAND_REG;
        case CPU_FMT_AC:    return operands_count == 2 && kind0 == OPERAND_REG && kind1 == OPERAND_CTRL_REG;
        case CPU_FMT_DB:    return operands_count == 2 && kind0 == OPERAND_DOT_REG && kind1 == OPERAND_REG;
        case CPU_FMT_AD:    return operands_count == 2 && kind0 == OPERAND_REG && kind1 == OPERAND_DOT_REG;
    }

    return false;
}

static bool assemble_inst(section_kind_t section_kind, command_t *cmd, size_t line_num, const char *line)
{
    assert(cmd != NULL);
    assert(line != NULL);

    if (sections[section_kind].size % CPU_INST_SIZE != 0) {
        fprintf(stderr, "An instruction must start at a multiple of %d bytes (use '.align') at line %zu: '%s'\n", CPU_INST_SIZE, line_num, line);
        return false;
    }

    operand_t operands[MAX_OPERANDS] = { 0 };
    for (size_t i = 0; i < cmd->args_count; ++i) {
        if (!parse_operand(cmd->args[i], &operands[i])) {
            fprintf(stderr, "Invalid argument %zu at line %zu: '%s'\n", i + 1, line_num, line);
            return false;
        }
    }

    bool is_emitted = true;

    /*
        An instruction holds 16 bits of an immediate value. `li` with a wider
        number or with a label, whose address is 32 bits wide, becomes two
        instructions: `li` with the lower half and `lih` with the upper half.
    */
    if (strcmp(cmd->token, "li") == 0 && cmd->args_count == 2 && operands[1].kind == OPERAND_REG &&
        (operands[0].kind == OPERAND_LABEL || (operands[0].kind == OPERAND_VALUE && !fits_imm(operands[0].value)))) {
        uint8_t reg_dst = operands[1].reg;
        if (reg_dst == CPU_REG_IP) {
            fprintf(stderr, "A 32-bit value takes two instructions to load, which 'ip' cannot hold between them, at line %zu: '%s'\n", line_num, line);
            return false;
        }

        uint32_t value = operands[0].value;
        if (operands[0].kind == OPERAND_LABEL) {
            is_emitted = is_emitted && add_fixup(ELF_RELOC_ASC2_LO16, operands[0].label, section_kind, line_num);
        }
        is_emitted = is_emitted && emit_inst(section_kind, CPU_MNEM_MOV | CPU_OPC_IMM, 0, reg_dst, (uint16_t) value);
        if (operands[0].kind == OPERAND_LABEL) {
            is_emitted = is_emitted && add_fixup(ELF_RELOC_ASC2_HI16, operands[0].label, section_kind, line_num);
        }
        is_emitted = is_emitted && emit_inst(section_kind, CPU_MNEM_LIH | CPU_OPC_IMM, 0, reg_dst, (uint16_t) (value >> 16));

        if (!is_emitted) {
            fprintf(stderr, "Maximum binary size has been reached at line %zu: '%s'\n", line_num, line);
        }
        return is_emitted;
    }

    // An instruction may have several forms; the kinds of the arguments select one
    bool is_known = false;
    const inst_enc_t *enc = NULL;
    for (size_t i = 0; i < CPU_INST; ++i) {
        if (strcmp(cmd->token, INST_ENC_TABLE[i].mnem) == 0) {
            is_known = true;
            if (operands_match(INST_ENC_TABLE[i].fmt, operands, cmd->args_count)) {
                enc = &INST_ENC_TABLE[i];
                break;
            }
        }
    }
    if (!is_known) {
        fprintf(stderr, "Unknown instruction at line %zu: '%s'\n", line_num, line);
        return false;
    }
    if (enc == NULL) {
        fprintf(stderr, "Invalid arguments for the instruction at line %zu: '%s'\n", line_num, line);
        return false;
    }

    uint32_t value = operands[0].value;
    switch (enc->fmt) {
        case CPU_FMT_NONE: {
            is_emitted = emit_inst(section_kind, enc->code, 0, 0, 0);
            break;
        }
        case CPU_FMT_A: {
            is_emitted = emit_inst(section_kind, enc->code, operands[0].reg, 0, 0);
            break;
        }
        case CPU_FMT_B: {
            is_emitted = emit_inst(section_kind, enc->code, 0, operands[0].reg, 0);
            break;
        }
        case CPU_FMT_AB:
        case CPU_FMT_CB:
        case CPU_FMT_AC:
        case CPU_FMT_DB:
        case CPU_FMT_AD: {
            is_emitted = emit_inst(section_kind, enc->code, operands[0].reg, operands[1].reg, 0);
            break;
        }
        case CPU_FMT_I:
        case CPU_FMT_IB: {
            bool is_valid = fits_imm(value);
            if (enc->code == (CPU_MNEM_INT | CPU_OPC_IMM)) {
                is_valid = value <= 0xff;
            } else if (enc->code == (CPU_MNEM_LIH | CPU_OPC_IMM)) {
                is_valid = value <= 0xffff; // `lih` takes the 16 bits as they are
            }
            if (!is_valid) {
                fprintf(stderr, "The value does not fit into the instruction (load it into a register with 'li' first) at line %zu: '%s'\n", line_num, line);
                return false;
            }

            is_emitted = emit_inst(section_kind, enc->code, 0, enc->fmt == CPU_FMT_IB ? operands[1].reg : 0, (uint16_t) value);
            break;
        }
        case CPU_FMT_REL: {
            // The distance to the label is filled in when all labels are known
            is_emitted = add_fixup(ELF_RELOC_ASC2_REL16, operands[0].label, section_kind, line_num) &&
                         emit_inst(section_kind, enc->code, 0, 0, 0);
            break;
        }
        case CPU_FMT_LOAD: {
            if (!fits_imm(operands[0].value)) {
                fprintf(stderr, "The displacement does not fit into the instruction at line %zu: '%s'\n", line_num, line);
                return false;
            }

            is_emitted = emit_inst(section_kind, enc->code, operands[0].reg, operands[1].reg, (uint16_t) operands[0].value);
            break;
        }
        case CPU_FMT_STORE: {
            if (!fits_imm(operands[1].value)) {
                fprintf(stderr, "The displacement does not fit into the instruction at line %zu: '%s'\n", line_num, line);
                return false;
            }

            is_emitted = emit_inst(section_kind, enc->code, operands[0].reg, operands[1].reg, (uint16_t) operands[1].value);
            break;
        }
        case CPU_FMT_ABC: {
            is_emitted = emit_inst(section_kind, enc->code, operands[0].reg, operands[1].reg, operands[2].reg);
            break;
        }
    }

    if (!is_emitted) {
        fprintf(stderr, "Maximum binary size has been reached at line %zu: '%s'\n", line_num, line);
    }
    return is_emitted;
}

static bool assemble_data(section_kind_t section_kind, command_t *cmd, size_t size, size_t line_num, const char *line)
{
    assert(cmd != NULL);
    assert(line != NULL);

    if (cmd->args_count < 1 || cmd->args_count > 2) {
        fprintf(stderr, "Invalid number of arguments at line %zu: '%s'\n", line_num, line);
        return false;
    }

    // A 4-byte value can be the address of a label
    uint32_t val = 0;
    bool is_label_val = size == 4 && is_label_name(cmd->args[0]);
    if (!is_label_val && (!parse_number(cmd->args[0], &val) || !fits_data(val, size))) {
        fprintf(stderr, "Invalid value at line %zu: '%s'\n", line_num, line);
        return false;
    }

    uint32_t cnt = 1;
    if (cmd->args_count == 2) {
        if (!parse_number(cmd->args[1], &cnt) || cnt > MAX_SECTION_SIZE) {
            fprintf(stderr, "Invalid count value at line %zu: '%s'\n", line_num, line);
            return false;
        }
    }

    for (uint32_t i = 0; i < cnt; ++i) {
        bool is_emitted = true;
        if (is_label_val) {
            is_emitted = add_fixup(ELF_RELOC_ASC2_32, cmd->args[0], section_kind, line_num);
        }
        is_emitted = is_emitted && emit_bytes(section_kind, val, size);

        if (!is_emitted) {
            fprintf(stderr, "Maximum binary size has been reached at line %zu: '%s'\n", line_num, line);
            return false;
        }
    }

    return true;
}

/*
    Emits the bytes of a string in double quotes, and a zero byte after them
    if `with_zero` is set. A backslash starts an escape sequence: `\n` (new
    line), `\t` (tab), `\r` (carriage return), `\0` (zero byte), `\\`, `\"`.
*/
static bool assemble_string(section_kind_t section_kind, command_t *cmd, bool with_zero, size_t line_num, const char *line)
{
    assert(cmd != NULL);
    assert(line != NULL);

    const char *text = cmd->text;
    size_t len = strlen(text);
    if (len < 2 || text[0] != '"' || text[len - 1] != '"') {
        fprintf(stderr, "A string in double quotes is expected at line %zu: '%s'\n", line_num, line);
        return false;
    }

    bool is_emitted = true;
    for (size_t i = 1; i < len - 1 && is_emitted; ++i) {
        char ch = text[i];
        if (ch == '"') { // a quote inside a string needs a backslash
            fprintf(stderr, "Invalid string at line %zu: '%s'\n", line_num, line);
            return false;
        }
        if (ch == '\\') {
            ++i;
            switch (i < len - 1 ? text[i] : '\0') {
                case 'n':  ch = '\n'; break;
                case 't':  ch = '\t'; break;
                case 'r':  ch = '\r'; break;
                case '0':  ch = '\0'; break;
                case '\\': ch = '\\'; break;
                case '"':  ch = '"';  break;
                default:
                    fprintf(stderr, "Invalid escape sequence in the string at line %zu: '%s'\n", line_num, line);
                    return false;
            }
        }

        is_emitted = emit_bytes(section_kind, (uint8_t) ch, 1);
    }
    if (with_zero) {
        is_emitted = is_emitted && emit_bytes(section_kind, 0, 1);
    }

    if (!is_emitted) {
        fprintf(stderr, "Maximum binary size has been reached at line %zu: '%s'\n", line_num, line);
        return false;
    }

    return true;
}

/*
    Puts the bytes of a file into the section as they are: `.incbin "path"`.
    The path is in double quotes, without escape sequences, and it starts from
    the directory where the assembler runs.
*/
static bool assemble_file(section_kind_t section_kind, command_t *cmd, size_t line_num, const char *line)
{
    assert(cmd != NULL);
    assert(line != NULL);

    char path[MAX_LINE_LEN];

    const char *text = cmd->text;
    size_t len = strlen(text);
    if (len < 3 || text[0] != '"' || text[len - 1] != '"' || memchr(text + 1, '"', len - 2) != NULL) {
        fprintf(stderr, "A path in double quotes is expected at line %zu: '%s'\n", line_num, line);
        return false;
    }
    memcpy(path, text + 1, len - 2);
    path[len - 2] = '\0';

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open the file '%s' at line %zu: '%s'\n", path, line_num, line);
        return false;
    }

    section_t *sec = &sections[section_kind];
    sec->size += fread(sec->bytes + sec->size, 1, MAX_SECTION_SIZE - sec->size, file);

    // The section is full, or the file has ended. One more byte tells which.
    bool is_whole = fgetc(file) == EOF && !ferror(file);
    fclose(file);

    if (!is_whole) {
        fprintf(stderr, "The file '%s' could not be read to its end, or the maximum binary size has been reached, at line %zu: '%s'\n", path, line_num, line);
        return false;
    }

    return true;
}

static void patch_imm(section_kind_t section_kind, size_t sec_pos, uint16_t imm)
{
    // The immediate value is the upper half of an instruction word
    sections[section_kind].bytes[sec_pos + 2] = (uint8_t) imm;
    sections[section_kind].bytes[sec_pos + 3] = (uint8_t) (imm >> 8);
}

static bool patch_rel(fixup_entry_t *fixup, uint32_t label_addr, uint32_t fixup_addr)
{
    assert(fixup != NULL);

    // The CPU counts the distance of a jump in instructions from the jump itself
    int64_t distance = (int64_t) label_addr - (int64_t) fixup_addr;
    if (distance % CPU_INST_SIZE != 0) {
        fprintf(stderr, "The label '%s' is not at a multiple of %d bytes for the jump at line %zu\n", fixup->label, CPU_INST_SIZE, fixup->line_num);
        return false;
    }

    distance /= CPU_INST_SIZE;
    if (distance < INT16_MIN || distance > INT16_MAX) {
        fprintf(stderr, "The label '%s' is too far for the jump at line %zu (load its address into a register with 'li' and jump through the register)\n", fixup->label, fixup->line_num);
        return false;
    }

    patch_imm(fixup->section_kind, fixup->sec_pos, (uint16_t) distance);

    return true;
}

/* Firmware Image Output */

static bool write_firmware(FILE *out_file)
{
    assert(out_file != NULL);

    static const uint32_t SECTION_ADDRS[SECTION_COUNT] = { FIRMWARE_TEXT_ADDR, FIRMWARE_DATA_ADDR };

    section_t *text_sec = &sections[SECTION_TEXT];
    section_t *data_sec = &sections[SECTION_DATA];

    if (text_sec->size > MAX_FIRMWARE_TEXT_SIZE) {
        fprintf(stderr, "The text section is too big: %zu bytes out of %d allowed\n", text_sec->size, MAX_FIRMWARE_TEXT_SIZE);
        return false;
    }
    if (data_sec->size > MAX_FIRMWARE_DATA_SIZE) {
        fprintf(stderr, "The data section is too big: %zu bytes out of %d allowed\n", data_sec->size, MAX_FIRMWARE_DATA_SIZE);
        return false;
    }

    // In a firmware image every section has a fixed address, so every label has one too
    for (size_t i = 0; i < fixups_count; ++i) {
        fixup_entry_t *fixup = &fixups[i];

        label_entry_t *label = get_label(fixup->label);
        if (label == NULL || !label->defined) {
            fprintf(stderr, "Undefined label '%s' at line %zu\n", fixup->label, fixup->line_num);
            return false;
        }

        uint32_t label_addr = SECTION_ADDRS[label->section_kind] + (uint32_t) label->sec_pos;
        uint32_t fixup_addr = SECTION_ADDRS[fixup->section_kind] + (uint32_t) fixup->sec_pos;
        uint8_t *bytes = sections[fixup->section_kind].bytes + fixup->sec_pos;

        if (fixup->kind == ELF_RELOC_ASC2_32) {
            bytes[0] = (uint8_t) label_addr;
            bytes[1] = (uint8_t) (label_addr >> 8);
            bytes[2] = (uint8_t) (label_addr >> 16);
            bytes[3] = (uint8_t) (label_addr >> 24);
        } else if (fixup->kind == ELF_RELOC_ASC2_LO16) {
            patch_imm(fixup->section_kind, fixup->sec_pos, (uint16_t) label_addr);
        } else if (fixup->kind == ELF_RELOC_ASC2_HI16) {
            patch_imm(fixup->section_kind, fixup->sec_pos, (uint16_t) (label_addr >> 16));
        } else if (fixup->kind == ELF_RELOC_ASC2_REL16) {
            if (!patch_rel(fixup, label_addr, fixup_addr)) {
                return false;
            }
        }
    }

    bin_fmt_header_t bin_fmt_header = {
        .magic = BIN_FMT_MAGIC,
        .text_sec_size = (uint32_t) text_sec->size,
        .data_sec_size = (uint32_t) data_sec->size
    };

    if (fwrite(&bin_fmt_header, sizeof(bin_fmt_header_t), 1, out_file) != 1) {
        perror("Failed to write the header to the compiled binary file");
        return false;
    }

    if ((text_sec->size > 0 && fwrite(text_sec->bytes, text_sec->size, 1, out_file) != 1) ||
        (data_sec->size > 0 && fwrite(data_sec->bytes, data_sec->size, 1, out_file) != 1)) {
        perror("Failed to write the machine code to the compiled binary file");
        return false;
    }

    return true;
}

/* Object File Output */

static bool write_object(FILE *out_file)
{
    assert(out_file != NULL);

    bool status = false;

    elf_sym_t *syms = NULL;
    char *strs = NULL;
    elf_rela_t *relas[SECTION_COUNT] = { NULL, NULL };
    size_t relas_count[SECTION_COUNT] = { 0, 0 };

    /*
        The linker decides where the sections go, so the addresses of labels
        are not known here. Every use of a label becomes a relocation for the
        linker, except a jump to a label of the same section: the distance
        between the two does not depend on where the section ends up.
    */
    for (size_t i = 0; i < fixups_count; ++i) {
        if (get_label(fixups[i].label) == NULL && add_label(fixups[i].label) == NULL) {
            fprintf(stderr, "Too many labels. The last one was found on line %zu\n", fixups[i].line_num);
            goto end;
        }
    }

    // The symbol table lists the labels: first the ones private to this file, then the ones shared with other files
    size_t syms_count = 1 + labels_count; // the first symbol is always empty
    size_t strs_size = 1;
    for (size_t i = 0; i < labels_count; ++i) {
        strs_size += strlen(labels[i].label) + 1;
    }

    syms = (elf_sym_t *) calloc(syms_count, sizeof(elf_sym_t));
    strs = (char *) calloc(strs_size, 1);
    relas[SECTION_TEXT] = (elf_rela_t *) calloc(fixups_count + 1, sizeof(elf_rela_t));
    relas[SECTION_DATA] = (elf_rela_t *) calloc(fixups_count + 1, sizeof(elf_rela_t));
    if (syms == NULL || strs == NULL || relas[SECTION_TEXT] == NULL || relas[SECTION_DATA] == NULL) {
        perror("Not enough memory to create the object file");
        goto end;
    }

    uint32_t sym_idx = 1;
    uint32_t local_syms_count = 1;
    size_t strs_pos = 1;
    for (int is_shared = 0; is_shared <= 1; ++is_shared) {
        for (size_t i = 0; i < labels_count; ++i) {
            label_entry_t *label = &labels[i];
            if ((label->global || !label->defined) != is_shared) {
                continue;
            }

            elf_sym_t *sym = &syms[sym_idx];
            sym->name = (uint32_t) strs_pos;
            sym->value = label->defined ? (uint32_t) label->sec_pos : 0;
            sym->size = 0;
            sym->info = (uint8_t) (((is_shared ? ELF_SYM_GLOBAL : ELF_SYM_LOCAL) << 4) | ELF_SYM_NOTYPE);
            sym->other = 0;
            sym->shndx = !label->defined ? 0 : (label->section_kind == SECTION_TEXT ? ELF_SEC_IDX_TEXT : ELF_SEC_IDX_DATA);

            strcpy(strs + strs_pos, label->label);
            strs_pos += strlen(label->label) + 1;

            label->sym_idx = sym_idx++;
        }
        if (!is_shared) {
            local_syms_count = sym_idx;
        }
    }

    for (size_t i = 0; i < fixups_count; ++i) {
        fixup_entry_t *fixup = &fixups[i];
        label_entry_t *label = get_label(fixup->label);

        if (fixup->kind == ELF_RELOC_ASC2_REL16 && label->defined && label->section_kind == fixup->section_kind) {
            if (!patch_rel(fixup, (uint32_t) label->sec_pos, (uint32_t) fixup->sec_pos)) {
                goto end;
            }
            continue;
        }

        elf_rela_t *rela = &relas[fixup->section_kind][relas_count[fixup->section_kind]++];
        rela->offset = (uint32_t) fixup->sec_pos;
        rela->info = (label->sym_idx << 8) | fixup->kind;
        rela->addend = 0;
    }

    // The file is the header, the contents of the sections one after another, and the table of section headers
    elf_sec_header_t sec_headers[ELF_SEC_IDX_COUNT] = { 0 };
    const void *sec_contents[ELF_SEC_IDX_COUNT] = { NULL };
    uint32_t offset = sizeof(elf_header_t);

    for (size_t i = 0; i < ELF_SEC_IDX_COUNT; ++i) {
        elf_sec_header_t *header = &sec_headers[i];
        header->name = ELF_SEC_NAME_POS[i];
        header->addralign = 4;

        switch (i) {
            case ELF_SEC_IDX_TEXT:
                header->type = ELF_SEC_PROGBITS;
                header->flags = ELF_SEC_FLAG_ALLOC | ELF_SEC_FLAG_EXEC;
                header->size = (uint32_t) sections[SECTION_TEXT].size;
                header->addralign = (uint32_t) sections[SECTION_TEXT].align;
                sec_contents[i] = sections[SECTION_TEXT].bytes;
                break;
            case ELF_SEC_IDX_DATA:
                header->type = ELF_SEC_PROGBITS;
                header->flags = ELF_SEC_FLAG_ALLOC | ELF_SEC_FLAG_WRITE;
                header->size = (uint32_t) sections[SECTION_DATA].size;
                header->addralign = (uint32_t) sections[SECTION_DATA].align;
                sec_contents[i] = sections[SECTION_DATA].bytes;
                break;
            case ELF_SEC_IDX_RELA_TEXT:
            case ELF_SEC_IDX_RELA_DATA: {
                section_kind_t section_kind = i == ELF_SEC_IDX_RELA_TEXT ? SECTION_TEXT : SECTION_DATA;
                header->type = ELF_SEC_RELA;
                header->flags = ELF_SEC_FLAG_INFO;
                header->size = (uint32_t) (relas_count[section_kind] * sizeof(elf_rela_t));
                header->link = ELF_SEC_IDX_SYMTAB;                                              // the symbols that the relocations name
                header->info = section_kind == SECTION_TEXT ? ELF_SEC_IDX_TEXT : ELF_SEC_IDX_DATA; // the section that the relocations change
                header->entsize = sizeof(elf_rela_t);
                sec_contents[i] = relas[section_kind];
                break;
            }
            case ELF_SEC_IDX_SYMTAB:
                header->type = ELF_SEC_SYMTAB;
                header->size = (uint32_t) (syms_count * sizeof(elf_sym_t));
                header->link = ELF_SEC_IDX_STRTAB; // the names of the symbols
                header->info = local_syms_count;   // the index of the first shared symbol
                header->entsize = sizeof(elf_sym_t);
                sec_contents[i] = syms;
                break;
            case ELF_SEC_IDX_STRTAB:
                header->type = ELF_SEC_STRTAB;
                header->size = (uint32_t) strs_size;
                header->addralign = 1;
                sec_contents[i] = strs;
                break;
            case ELF_SEC_IDX_SHSTRTAB:
                header->type = ELF_SEC_STRTAB;
                header->size = sizeof(ELF_SEC_NAMES);
                header->addralign = 1;
                sec_contents[i] = ELF_SEC_NAMES;
                break;
            default:
                header->addralign = 0;
                break;
        }

        if (i != ELF_SEC_IDX_NULL) {
            offset = (offset + 3) & ~3u; // every section starts at a multiple of 4 bytes in the file
            header->offset = offset;
            offset += header->size;
        }
    }
    offset = (offset + 3) & ~3u;

    elf_header_t elf_header = {
        .ident = { 0x7f, 'E', 'L', 'F', ELF_CLASS_32, ELF_DATA_LSB, ELF_VERSION },
        .type = ELF_TYPE_REL,
        .machine = ELF_MACHINE_ASC2,
        .version = ELF_VERSION,
        .shoff = offset,
        .ehsize = sizeof(elf_header_t),
        .shentsize = sizeof(elf_sec_header_t),
        .shnum = ELF_SEC_IDX_COUNT,
        .shstrndx = ELF_SEC_IDX_SHSTRTAB
    };

    static const uint8_t PADDING[4] = { 0 };
    bool is_written = fwrite(&elf_header, sizeof(elf_header), 1, out_file) == 1;
    uint32_t file_pos = sizeof(elf_header_t);
    for (size_t i = 1; i < ELF_SEC_IDX_COUNT && is_written; ++i) {
        elf_sec_header_t *header = &sec_headers[i];
        if (header->offset > file_pos) {
            is_written = fwrite(PADDING, header->offset - file_pos, 1, out_file) == 1;
        }
        if (is_written && header->size > 0) {
            is_written = fwrite(sec_contents[i], header->size, 1, out_file) == 1;
        }
        file_pos = header->offset + header->size;
    }
    if (is_written && offset > file_pos) {
        is_written = fwrite(PADDING, offset - file_pos, 1, out_file) == 1;
    }
    is_written = is_written && fwrite(sec_headers, sizeof(sec_headers), 1, out_file) == 1;
    if (!is_written) {
        perror("Failed to write the object file");
        goto end;
    }

    status = true;

end:
    free(syms);
    free(strs);
    free(relas[SECTION_TEXT]);
    free(relas[SECTION_DATA]);

    return status;
}

int main(int argc, char **argv)
{
    int status = EXIT_FAILURE;

    FILE *in_file  = NULL;
    FILE *out_file = NULL;

    bool is_object = argc == 4 && strcmp(argv[1], "-c") == 0;
    if (argc != 3 && !is_object) {
        fprintf(
            stderr,
            "Usage:\n"
                "\t%s [-c] <path to the assembly file> <path with a file name of the output file>\n"
                "\n"
                "\tWithout options the output is a firmware image for the boot ROM.\n"
                "\tWith '-c' the output is an ELF32 object file for the linker.\n",
            PROGRAM_NAME
        );
        goto end;
    }

    const char *in_file_path = (const char *) argv[argc - 2];
    in_file = fopen(in_file_path, "rb");
    if (in_file == NULL) {
        perror("Failed to open the assembly file");
        goto end;
    }

    for (size_t i = 0; i < SECTION_COUNT; ++i) {
        sections[i].bytes = (uint8_t *) malloc(MAX_SECTION_SIZE);
        if (sections[i].bytes == NULL) {
            perror("Not enough memory to create internal data structures to process the assembly file");
            goto end;
        }
        sections[i].size = 0;
        sections[i].align = CPU_INST_SIZE;
    }

    section_kind_t section_kind = SECTION_TEXT;
    section_t *sec = &sections[section_kind];

    size_t line_num = 0;

    char input_buffer[MAX_LINE_LEN];
    while (fgets(input_buffer, sizeof(input_buffer), in_file)) {
        char *line = trim(input_buffer);
        ++line_num;

        command_t cmd;
        parse_line(line, &cmd);

        if (cmd.type == INVALID) {
            fprintf(stderr, "Invalid code at line %zu: '%s'\n", line_num, line);
            goto end;
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "section") == 0) {
            if (cmd.args_count == 1 && strcmp(cmd.args[0], ".data") == 0) {
                section_kind = SECTION_DATA;
            } else if (cmd.args_count == 1 && strcmp(cmd.args[0], ".text") == 0) {
                section_kind = SECTION_TEXT;
            } else {
                fprintf(stderr, "Invalid section name at line %zu: '%s'\n", line_num, line);
                goto end;
            }
            sec = &sections[section_kind];
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "align") == 0) {
            uint32_t align;
            if (cmd.args_count != 1 || !parse_number(cmd.args[0], &align) || align == 0 || align > PAGE_SIZE) {
                fprintf(stderr, "Invalid alignment value at line %zu: '%s'\n", line_num, line);
                goto end;
            }

            size_t rem = sec->size % (size_t) align;
            if (rem != 0) {
                size_t pad = (size_t) align - rem;
                if (sec->size + pad > MAX_SECTION_SIZE) {
                    fprintf(stderr, "Maximum binary size has been reached at line %zu: '%s'\n", line_num, line);
                    goto end;
                }

                memset(sec->bytes + sec->size, 0, pad);
                sec->size += pad;
            }
            if (align > sec->align) {
                sec->align = align;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "byte") == 0) {
            if (!assemble_data(section_kind, &cmd, 1, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "short") == 0) {
            if (!assemble_data(section_kind, &cmd, 2, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "long") == 0) {
            if (!assemble_data(section_kind, &cmd, 4, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "ascii") == 0) {
            if (!assemble_string(section_kind, &cmd, false, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "asciz") == 0) {
            if (!assemble_string(section_kind, &cmd, true, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "incbin") == 0) {
            if (!assemble_file(section_kind, &cmd, line_num, line)) {
                goto end;
            }
        } else if (cmd.type == DIRECTIVE && strcmp(cmd.token, "global") == 0) {
            if (cmd.args_count != 1 || !is_label_name(cmd.args[0])) {
                fprintf(stderr, "Invalid label name at line %zu: '%s'\n", line_num, line);
                goto end;
            }

            label_entry_t *label = get_label(cmd.args[0]);
            if (label == NULL) {
                label = add_label(cmd.args[0]);
            }
            if (label == NULL) {
                fprintf(stderr, "Too many labels. The last one was found on line %zu: '%s'\n", line_num, line);
                goto end;
            }
            label->global = true;
        } else if (cmd.type == DIRECTIVE) {
            fprintf(stderr, "Unknown directive at line %zu: '%s'\n", line_num, line);
            goto end;
        } else if (cmd.type == LABEL) {
            uint8_t reg_code;
            if (get_reg_code(cmd.token, REG_ENC_TABLE, CPU_REGS, &reg_code) ||
                get_reg_code(cmd.token, CTRL_REG_ENC_TABLE, CPU_CTRL_REGS, &reg_code) ||
                get_reg_code(cmd.token, DOT_REG_ENC_TABLE, CPU_DOT_REGS, &reg_code) ||
                !is_label_name(cmd.token)) {
                fprintf(stderr, "Invalid label name at line %zu: '%s'\n", line_num, line);
                goto end;
            }

            label_entry_t *label = get_label(cmd.token);
            if (label != NULL && label->defined) {
                fprintf(stderr, "The label is defined twice. The second time at line %zu: '%s'\n", line_num, line);
                goto end;
            }
            if (label == NULL) {
                label = add_label(cmd.token);
            }
            if (label == NULL) {
                fprintf(stderr, "Too many labels. The last one was found on line %zu: '%s'\n", line_num, line);
                goto end;
            }

            label->defined = true;
            label->section_kind = section_kind;
            label->sec_pos = sec->size;
        } else if (cmd.type == INSTRUCTION) {
            if (!assemble_inst(section_kind, &cmd, line_num, line)) {
                goto end;
            }
        }
    }

    // The output file is created only now, so a failed run leaves no half-written file behind
    const char *out_file_path = (const char *) argv[argc - 1];
    out_file = fopen(out_file_path, "wb");
    if (out_file == NULL) {
        perror("Failed to create the output file");
        goto end;
    }

    if (is_object ? !write_object(out_file) : !write_firmware(out_file)) {
        fclose(out_file);
        out_file = NULL;
        remove(out_file_path);
        goto end;
    }

    status = EXIT_SUCCESS;

end:
    for (size_t i = 0; i < SECTION_COUNT; ++i) {
        if (sections[i].bytes != NULL) {
            free(sections[i].bytes);
            sections[i].bytes = NULL;
            sections[i].size = 0;
        }
    }

    if (out_file != NULL) {
        fclose(out_file);
        out_file = NULL;
    }

    if (in_file != NULL) {
        fclose(in_file);
        in_file = NULL;
    }

    return status;
}
