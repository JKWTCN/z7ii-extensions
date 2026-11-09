/* SPDX-License-Identifier: MIT */
.syntax unified
.thumb
.section .text.entry,"ax",%progbits
.global extension_init_hook
.thumb_func
extension_init_hook:
    push {r4, lr}
    ldr r0, =0xb1819af4
    ldr r1, =0x1044e158
    ldr r3, =0x10361083
    blx r3
    bl extension_boot
    pop.w {r4, lr}
    ldr pc, =0x1044e01f
.ltorg

.global extension_grid_source
.thumb_func
extension_grid_source:
    push {r1-r3, lr}
    ldr r0, =0x1094d6ac
    bl extension_grid_table
    pop.w {r1-r3, lr}
    uxtb r7, r1
    movw r1, #0x464
    ldr pc, =0x105afc4d
.ltorg

.global extension_grid_enable
.thumb_func
extension_grid_enable:
    push {r1-r3, lr}
    movw r0, #0x112d
    ldr r3, =0x1063c7d5
    blx r3
    bl extension_grid_enabled
    pop.w {r1-r3, lr}
    cmp r0, #1
    ldr pc, =0x105b0119
.ltorg
