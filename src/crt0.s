	.syntax unified
	.include "constants/gba_constants.inc"

	.arm

	.align 2, 0
Init::
	mov r0, #0x12
	msr cpsr_cf, r0
	ldr sp, sp_irq
	mov r0, #0x1f
	msr cpsr_cf, r0
	ldr sp, sp_sys
	ldr r1, =0x3007FFC
	adr r0, IntrMain
	str r0, [r1]
	mov r0, #255 @ RESET_ALL
	svc #1 << 16
	ldr r0, =AgbMain + 1
	mov lr, pc
	bx r0

	.align 2, 0
sp_sys: .word IWRAM_END - 0x1c0
sp_irq: .word IWRAM_END - 0x60

	.pool

	.arm
	.align 2, 0
IntrMain::
	mov r3, #0x4000000
	add r3, r3, #0x200
	ldr r2, [r3]
	ldrh r1, [r3, #0x8]
	mrs r0, spsr
	push {r0-r3,lr}
	mov r0, #0
	strh r0, [r3, #0x8]
	and r1, r2, r2, lsr #16
	mov r12, #0
	ands r0, r1, #0x4
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	mov r0, 0x1
	strh r0, [r3, #0x8]
	ands r0, r1, #0x80
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x40
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x2
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x1
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x8
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x10
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x20
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x100
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x200
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x400
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #800
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x1000
	bne IntrMain_FoundIntr
	add r12, r12, 0x4
	ands r0, r1, #0x2000
	strbne r0, [r3, #-0x17c]
	bne . @ spin
IntrMain_FoundIntr:
	strh r0, [r3, 0x2]
	bic r2, r2, r0
	ldr r0, =gSTWIStatus
	ldr r0, [r0]
	ldrb r0, [r0, 0xA]
	mov r1, #0x8
	lsl r0, r1, r0
	orr r0, r0, #0x2000
	orr r1, r0, #0xc6
	and r1, r1, r2
	strh r1, [r3]
	mrs r3, cpsr
	bic r3, r3, #0xdf
	orr r3, r3, #0x1f
	msr cpsr_cf, r3
	ldr r1, =gIntrTable
	ldr r0, [r1, r12]
	push {lr}
	adr lr, IntrMain_RetAddr
	bx r0
IntrMain_RetAddr:
	pop {lr}
	mrs r3, cpsr
	bic r3, r3, #0xdf
	orr r3, r3, #0x92
	msr cpsr_cf, r3
	pop {r0-r3,lr}
	strh r2, [r3]
	strh r1, [r3, #0x8]
	msr spsr_cf, r0
	bx lr

	.pool

	.align 2, 0 @ Don't pad with nop.
