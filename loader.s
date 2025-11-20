/*

MIT License

Copyright (c) 2024 kekeimiku

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

.section __DATA, __data

.global ___shellcode_start
.global ___shellcode_end
.global ___patch_pthread_create
.global ___patch_sandbox_consume
.global ___patch_dlopen
.global ___data_payload_path
.global ___data_sandbox_token

___shellcode_start:
	sub sp, sp, #0x30
	stp x29, x30, [sp, #0x20]
	add x29, sp, #0x20

	stur w0, [x29, #-0x4]
	str  x1, [sp, #0x10]

	add    x0, sp, #0x8
	mov    x8, #0
	str    x8, [sp, #0x8]
	mov    x1, x8
	adr    x2, __thread_entry
	paciza x2
	mov    x3, x8

	adr x9, ___patch_pthread_create
	ldr x9, [x9]
	blr x9

	movz x0, #0x4e45
	movk x0, #0x444f, lsl #16

	b .

	.align 4
__thread_entry:
	pacibsp
	sub sp, sp, #0x30
	stp x29, x30, [sp, #0x20]
	add x29, sp, #0x20

	stur w0, [x29, #-0x4]
	str  x1, [sp, #0x10]

	adr x0, ___data_sandbox_token
	adr x9, ___patch_sandbox_consume
	ldr x9, [x9]
	blr x9

	mov x1, #1
	adr x0, ___data_payload_path
	adr x9, ___patch_dlopen
	ldr x9, [x9]
	blr x9

	ldp x29, x30, [sp, #0x20]
	add sp, sp, #0x30
	retab

	.align 3
___patch_pthread_create:
	.quad 0x0

	.align 3
___patch_sandbox_consume:
	.quad 0x0

	.align 3
___patch_dlopen:
	.quad 0x0

	.align 3
___data_payload_path:
	.zero 0x80

	.align 3
___data_sandbox_token:
	.zero 0x100

___shellcode_end:
