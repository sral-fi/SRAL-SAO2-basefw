/* Startup code for STM32C011F6P6 (Cortex-M0+) */
/* TODO: Complete vector table when CMSIS files are available */

    .syntax unified
    .cpu cortex-m0plus
    .fpu softvfp
    .thumb

/* Vector Table - Minimal placeholder */
    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word   _estack                     /* Top of Stack */
    .word   Reset_Handler               /* Reset Handler */
    .word   NMI_Handler                 /* NMI Handler */
    .word   HardFault_Handler           /* Hard Fault Handler */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   SVC_Handler                 /* SVCall Handler */
    .word   0                           /* Reserved */
    .word   0                           /* Reserved */
    .word   PendSV_Handler              /* PendSV Handler */
    .word   SysTick_Handler             /* SysTick Handler */

    /* TODO: Add STM32C011 specific external interrupts */
    /* Placeholder for now - add complete vector table later */
    .rept 32
    .word   Default_Handler
    .endr

/* Reset Handler */
    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr   r0, =_estack
    mov   sp, r0                    /* Set stack pointer */

/* Copy the data segment initializers from flash to SRAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b LoopCopyDataInit

CopyDataInit:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

LoopCopyDataInit:
    adds r4, r0, r3
    cmp r4, r1
    bcc CopyDataInit

/* Zero fill the bss segment */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b LoopFillZerobss

FillZerobss:
    str  r3, [r2]
    adds r2, r2, #4

LoopFillZerobss:
    cmp r2, r4
    bcc FillZerobss

/* Call main program */
    bl main
    bx lr

    .size Reset_Handler, .-Reset_Handler

/* Default interrupt handler */
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    b Infinite_Loop
    .size Default_Handler, .-Default_Handler

/* Weak aliases for each exception handler */
    .weak      NMI_Handler
    .thumb_set NMI_Handler,Default_Handler

    .weak      HardFault_Handler
    .thumb_set HardFault_Handler,Default_Handler

    .weak      SVC_Handler
    .thumb_set SVC_Handler,Default_Handler

    .weak      PendSV_Handler
    .thumb_set PendSV_Handler,Default_Handler

    .weak      SysTick_Handler
    .thumb_set SysTick_Handler,Default_Handler
