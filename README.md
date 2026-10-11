# shab-polyglot

A small register-based virtual machine written in C. Cross-platform, single binary, no dependencies.

## What it is

shab-polyglot executes programs written in a minimal intermediate representation (IR). It is a teaching VM: small enough to read in one sitting, complete enough to run real programs.

## Features

- 22 instructions: arithmetic, comparison, control flow, memory
- Register-based (256 registers)
- Label-based jumps
- Bounds-checked memory (65536 cells)
- Division by zero protection
- Step limit (prevents infinite loops)
- Clear error messages with line numbers
- Cross-platform: Linux, macOS, Windows (x86_64 + arm64)

## Install

Download the binary for your platform from the Releases page, or build from source:

    make
    sudo install -m 0755 shab /usr/local/bin/

## Usage

    shab PROGRAM.ir [step_limit]

Default step limit is 1000000. Example:

    shab example.ir

## Instruction set

    const rN INT       load immediate
    mov rD rA          copy register
    add rD rA rB       rD = rA + rB
    sub rD rA rB       rD = rA - rB
    mul rD rA rB       rD = rA * rB
    div rD rA rB       rD = rA / rB
    mod rD rA rB       rD = rA % rB
    neg rD rA          rD = -rA
    cmp rA rB          compare, sets flag
    print rN           print register
    label NAME         define jump target
    jmp NAME           unconditional jump
    jz NAME            jump if equal
    jnz NAME           jump if not equal
    jl NAME            jump if less
    jg NAME            jump if greater
    jle NAME           jump if less or equal
    jge NAME           jump if greater or equal
    alloc rN SIZE      allocate memory region, address in rN
    store rB rO rV     memory[rB + rO] = rV
    load rD rB rO      rD = memory[rB + rO]
    halt               end program

## Example program

    # (40 + 2) * 3
    const r0 40
    const r1 2
    add r2 r0 r1
    const r3 3
    mul r4 r2 r3
    print r2
    print r4
    halt

Output:

    r2 = 42
    r4 = 126

## License

MIT
