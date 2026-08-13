.syntax unified
.cpu cortex-m3
.thumb
.global g_pfnVectors
.global Reset_Handler
.extern boot_main
.section .isr_vector,"a",%progbits
.type g_pfnVectors,%object
g_pfnVectors:
 .word _estack
 .word Reset_Handler
 .rept 58
 .word Default_Handler
 .endr
.size g_pfnVectors,.-g_pfnVectors
.section .text.Reset_Handler,"ax",%progbits
.thumb_func
Reset_Handler:
 ldr r0,=_sidata
 ldr r1,=_sdata
 ldr r2,=_edata
1: cmp r1,r2
 bcc 2f
 b 3f
2: ldr r3,[r0],#4
 str r3,[r1],#4
 b 1b
3: ldr r1,=_sbss
 ldr r2,=_ebss
 movs r3,#0
4: cmp r1,r2
 bcc 5f
 b 6f
5: str r3,[r1],#4
 adds r1,#4
 b 4b
6: bl boot_main
 b .
.thumb_func
Default_Handler:
 b .
