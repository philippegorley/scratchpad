%macro pnum 1
    section .data
        .str db %1,0
    section .text
        mov rdi, fmt4
        mov rsi, .str
        mov rdx, [a]
        mov rcx, [b]
        mov r8, [c]
        mov rax, 0
        call printf
%endmacro

section .data
    a: dq 3
    b: dq 4
    fmt4: db `%s, a=%ld, b=%ld, c=%ld\n`,0

section .bss
    c: resq 1

section .text
    global _start
    extern printf, exit

_start:
    mov rax, 5
    mov [c], rax
    pnum "c=5"
add:
    mov rax, [a]
    add rax, [b]
    mov [c], rax
    pnum "c=a+b"
sub:
    mov rax, [a]
    sub rax, [b]
    mov [c], rax
    pnum "c=a-b"
mul:
    mov rax, [a]
    imul rax, [b]
    mov [c], rax
    pnum "c=a*b"
div:
    mov rax, [c]
    mov rdx, 0          ; upper half of dividend = 0
    idiv qword [a]      ; divide edx:rax by a
    mov [c], rax
    pnum "c=c/a"

    xor rax, rax
    xor rdi, rdi
    call exit
