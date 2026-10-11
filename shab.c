/* shab.c — shab-polyglot virtual machine */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#ifndef _WIN32
#include <sys/utsname.h>
#endif

#define MAX_REG     256
#define MAX_LABELS  512
#define MAX_INSTR   8192
#define MAX_MEM     65536
#define MAX_LINE    512

typedef enum {
    OP_CONST, OP_MOV,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
    OP_CMP,
    OP_PRINT,
    OP_LABEL, OP_JMP, OP_JZ, OP_JNZ, OP_JL, OP_JG, OP_JLE, OP_JGE,
    OP_ALLOC, OP_STORE, OP_LOAD,
    OP_HALT
} OpCode;

typedef struct {
    OpCode  op;
    int     a, b, c;
    int64_t imm;
    char    label[64];
    int     line;
} Instr;

static Instr   prog[MAX_INSTR];
static int     prog_len = 0;

static struct { char name[64]; int index; } labels[MAX_LABELS];
static int     label_count = 0;

static int64_t regs[MAX_REG];
static int64_t mem[MAX_MEM];
static int     mem_top = 0;
static int     cmp_flag = 0;

static int parse_reg(const char *tok) {
    if (!tok || (tok[0] != 'r' && tok[0] != 'R')) return -1;
    if (tok[1] == '\0') return -1;
    char *end;
    long n = strtol(tok + 1, &end, 10);
    if (*end != '\0') return -1;
    if (n < 0 || n >= MAX_REG) return -1;
    return (int)n;
}

static int parse_int(const char *tok, int64_t *out) {
    if (!tok) return -1;
    char *end;
    long long v = strtoll(tok, &end, 0);
    if (*end != '\0') return -1;
    *out = (int64_t)v;
    return 0;
}

static int find_label(const char *name) {
    for (int i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0) return labels[i].index;
    return -1;
}

static int add_label(const char *name, int idx) {
    if (label_count >= MAX_LABELS) return -1;
    if (find_label(name) >= 0) return -1;
    snprintf(labels[label_count].name, sizeof(labels[0].name), "%s", name);
    labels[label_count].index = idx;
    label_count++;
    return 0;
}

static int parse_program(FILE *fp) {
    char line[MAX_LINE];
    int  lineno = 0;
    prog_len = 0;
    label_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        char *tok = strtok(p, " \t");
        if (!tok) continue;

        Instr ins;
        memset(&ins, 0, sizeof(ins));
        ins.line = lineno;

        if (strcmp(tok, "const") == 0) {
            char *rt = strtok(NULL, " \t");
            char *it = strtok(NULL, " \t");
            int r = parse_reg(rt);
            if (r < 0 || parse_int(it, &ins.imm) < 0) {
                fprintf(stderr, "Line %d: expected const rN INTEGER\n", lineno);
                return -1;
            }
            ins.op = OP_CONST; ins.a = r;
        } else if (strcmp(tok, "mov") == 0) {
            char *rd = strtok(NULL, " \t");
            char *ra = strtok(NULL, " \t");
            int d = parse_reg(rd), a = parse_reg(ra);
            if (d < 0 || a < 0) {
                fprintf(stderr, "Line %d: expected mov rD rA\n", lineno);
                return -1;
            }
            ins.op = OP_MOV; ins.a = d; ins.b = a;
        } else if (strcmp(tok, "add") == 0 || strcmp(tok, "sub") == 0 ||
                   strcmp(tok, "mul") == 0 || strcmp(tok, "div") == 0 ||
                   strcmp(tok, "mod") == 0) {
            char *rd = strtok(NULL, " \t");
            char *ra = strtok(NULL, " \t");
            char *rb = strtok(NULL, " \t");
            int d = parse_reg(rd), a = parse_reg(ra), b = parse_reg(rb);
            if (d < 0 || a < 0 || b < 0) {
                fprintf(stderr, "Line %d: expected %s rD rA rB\n", lineno, tok);
                return -1;
            }
            if (strcmp(tok, "add") == 0) ins.op = OP_ADD;
            else if (strcmp(tok, "sub") == 0) ins.op = OP_SUB;
            else if (strcmp(tok, "mul") == 0) ins.op = OP_MUL;
            else if (strcmp(tok, "div") == 0) ins.op = OP_DIV;
            else ins.op = OP_MOD;
            ins.a = d; ins.b = a; ins.c = b;
        } else if (strcmp(tok, "neg") == 0) {
            char *rd = strtok(NULL, " \t");
            char *ra = strtok(NULL, " \t");
            int d = parse_reg(rd), a = parse_reg(ra);
            if (d < 0 || a < 0) {
                fprintf(stderr, "Line %d: expected neg rD rA\n", lineno);
                return -1;
            }
            ins.op = OP_NEG; ins.a = d; ins.b = a;
        } else if (strcmp(tok, "cmp") == 0) {
            char *ra = strtok(NULL, " \t");
            char *rb = strtok(NULL, " \t");
            int a = parse_reg(ra), b = parse_reg(rb);
            if (a < 0 || b < 0) {
                fprintf(stderr, "Line %d: expected cmp rA rB\n", lineno);
                return -1;
            }
            ins.op = OP_CMP; ins.a = a; ins.b = b;
        } else if (strcmp(tok, "print") == 0) {
            char *rt = strtok(NULL, " \t");
            int r = parse_reg(rt);
            if (r < 0) {
                fprintf(stderr, "Line %d: expected print rN\n", lineno);
                return -1;
            }
            ins.op = OP_PRINT; ins.a = r;
        } else if (strcmp(tok, "label") == 0) {
            char *name = strtok(NULL, " \t");
            if (!name) {
                fprintf(stderr, "Line %d: expected label NAME\n", lineno);
                return -1;
            }
            snprintf(ins.label, sizeof(ins.label), "%s", name);
            ins.op = OP_LABEL;
        } else if (strcmp(tok, "jmp") == 0 || strcmp(tok, "jz") == 0 ||
                   strcmp(tok, "jnz") == 0 || strcmp(tok, "jl") == 0 ||
                   strcmp(tok, "jg") == 0 || strcmp(tok, "jle") == 0 ||
                   strcmp(tok, "jge") == 0) {
            char *name = strtok(NULL, " \t");
            if (!name) {
                fprintf(stderr, "Line %d: expected %s NAME\n", lineno, tok);
                return -1;
            }
            if (strcmp(tok, "jmp") == 0) ins.op = OP_JMP;
            else if (strcmp(tok, "jz") == 0) ins.op = OP_JZ;
            else if (strcmp(tok, "jnz") == 0) ins.op = OP_JNZ;
            else if (strcmp(tok, "jl") == 0) ins.op = OP_JL;
            else if (strcmp(tok, "jg") == 0) ins.op = OP_JG;
            else if (strcmp(tok, "jle") == 0) ins.op = OP_JLE;
            else ins.op = OP_JGE;
            snprintf(ins.label, sizeof(ins.label), "%s", name);
        } else if (strcmp(tok, "alloc") == 0) {
            char *rt = strtok(NULL, " \t");
            char *sz = strtok(NULL, " \t");
            int r = parse_reg(rt);
            if (r < 0 || parse_int(sz, &ins.imm) < 0 || ins.imm < 0) {
                fprintf(stderr, "Line %d: expected alloc rN SIZE\n", lineno);
                return -1;
            }
            ins.op = OP_ALLOC; ins.a = r;
        } else if (strcmp(tok, "store") == 0) {
            char *rb = strtok(NULL, " \t");
            char *ro = strtok(NULL, " \t");
            char *rv = strtok(NULL, " \t");
            int b = parse_reg(rb), o = parse_reg(ro), v = parse_reg(rv);
            if (b < 0 || o < 0 || v < 0) {
                fprintf(stderr, "Line %d: expected store rBase rOff rVal\n", lineno);
                return -1;
            }
            ins.op = OP_STORE; ins.a = b; ins.b = o; ins.c = v;
        } else if (strcmp(tok, "load") == 0) {
            char *rd = strtok(NULL, " \t");
            char *rb = strtok(NULL, " \t");
            char *ro = strtok(NULL, " \t");
            int d = parse_reg(rd), b = parse_reg(rb), o = parse_reg(ro);
            if (d < 0 || b < 0 || o < 0) {
                fprintf(stderr, "Line %d: expected load rDest rBase rOff\n", lineno);
                return -1;
            }
            ins.op = OP_LOAD; ins.a = d; ins.b = b; ins.c = o;
        } else if (strcmp(tok, "halt") == 0) {
            ins.op = OP_HALT;
        } else {
            fprintf(stderr, "Line %d: unknown instruction '%s'\n", lineno, tok);
            return -1;
        }

        if (prog_len >= MAX_INSTR) {
            fprintf(stderr, "Program too large (max %d)\n", MAX_INSTR);
            return -1;
        }
        prog[prog_len] = ins;
        if (ins.op == OP_LABEL) add_label(ins.label, prog_len);
        prog_len++;
    }

    if (prog_len == 0 || prog[prog_len - 1].op != OP_HALT) {
        fprintf(stderr, "Error: program must end with halt\n");
        return -1;
    }
    return 0;
}

static int execute(int step_limit) {
    int pc = 0;
    int steps = 0;
    while (pc < prog_len) {
        if (steps >= step_limit) {
            fprintf(stderr, "Error: execution step limit reached\n");
            return -1;
        }
        steps++;
        Instr *ins = &prog[pc];
        switch (ins->op) {
        case OP_CONST: regs[ins->a] = ins->imm; pc++; break;
        case OP_MOV:   regs[ins->a] = regs[ins->b]; pc++; break;
        case OP_ADD:   regs[ins->a] = regs[ins->b] + regs[ins->c]; pc++; break;
        case OP_SUB:   regs[ins->a] = regs[ins->b] - regs[ins->c]; pc++; break;
        case OP_MUL:   regs[ins->a] = regs[ins->b] * regs[ins->c]; pc++; break;
        case OP_DIV:
            if (regs[ins->c] == 0) {
                fprintf(stderr, "Error: division by zero (line %d)\n", ins->line);
                return -1;
            }
            regs[ins->a] = regs[ins->b] / regs[ins->c]; pc++; break;
        case OP_MOD:
            if (regs[ins->c] == 0) {
                fprintf(stderr, "Error: modulo by zero (line %d)\n", ins->line);
                return -1;
            }
            regs[ins->a] = regs[ins->b] % regs[ins->c]; pc++; break;
        case OP_NEG:   regs[ins->a] = -regs[ins->b]; pc++; break;
        case OP_CMP:
            if (regs[ins->a] < regs[ins->b]) cmp_flag = -1;
            else if (regs[ins->a] > regs[ins->b]) cmp_flag = 1;
            else cmp_flag = 0;
            pc++; break;
        case OP_PRINT:
            printf("r%d = %" PRId64 "\n", ins->a, regs[ins->a]); pc++; break;
        case OP_LABEL: pc++; break;
        case OP_JMP: {
            int t = find_label(ins->label);
            if (t < 0) { fprintf(stderr, "Error: label '%s' not found\n", ins->label); return -1; }
            pc = t; break;
        }
        case OP_JZ:  { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag == 0) ? t : pc + 1; break; }
        case OP_JNZ: { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag != 0) ? t : pc + 1; break; }
        case OP_JL:  { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag <  0) ? t : pc + 1; break; }
        case OP_JG:  { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag >  0) ? t : pc + 1; break; }
        case OP_JLE: { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag <= 0) ? t : pc + 1; break; }
        case OP_JGE: { int t = find_label(ins->label); if (t < 0) { fprintf(stderr, "label '%s' not found\n", ins->label); return -1; } pc = (cmp_flag >= 0) ? t : pc + 1; break; }
        case OP_ALLOC:
            regs[ins->a] = mem_top;
            mem_top += (int)ins->imm;
            if (mem_top > MAX_MEM) { fprintf(stderr, "Error: out of memory\n"); return -1; }
            pc++; break;
        case OP_STORE: {
            int64_t base = regs[ins->a], off = regs[ins->b];
            if (base + off < 0 || base + off >= MAX_MEM) { fprintf(stderr, "Error: store OOB\n"); return -1; }
            mem[base + off] = regs[ins->c]; pc++; break;
        }
        case OP_LOAD: {
            int64_t base = regs[ins->b], off = regs[ins->c];
            if (base + off < 0 || base + off >= MAX_MEM) { fprintf(stderr, "Error: load OOB\n"); return -1; }
            regs[ins->a] = mem[base + off]; pc++; break;
        }
        case OP_HALT: return steps;
        default: fprintf(stderr, "Error: bad opcode\n"); return -1;
        }
    }
    return steps;
}

static void print_banner(void) {
    printf("Compile-time architecture: ");
#if defined(__aarch64__)
    printf("ARM64 / AArch64");
#elif defined(__x86_64__)
    printf("x86_64");
#elif defined(__arm__)
    printf("ARM32");
#elif defined(__i386__)
    printf("x86");
#else
    printf("unknown");
#endif
    printf("\n");
#ifdef _WIN32
    printf("Kernel system: Windows\n");
    printf("Kernel machine: x86_64\n");
#else
    struct utsname u;
    if (uname(&u) == 0) {
        printf("Kernel system: %s\n", u.sysname);
        printf("Kernel machine: %s\n", u.machine);
    }
#endif
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s PROGRAM.ir [step_limit]\n", argv[0]);
        return 1;
    }
    int step_limit = 1000000;
    if (argc >= 3) {
        step_limit = atoi(argv[2]);
        if (step_limit <= 0) { fprintf(stderr, "Invalid step limit\n"); return 1; }
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror(argv[1]); return 1; }
    if (parse_program(fp) != 0) { fclose(fp); return 1; }
    fclose(fp);

    print_banner();
    int steps = execute(step_limit);
    if (steps < 0) return 1;
    printf("Execution complete: %d instruction(s)\n", steps);
    return 0;
}
