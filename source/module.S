// SPDX-License-Identifier: AGPL-3.0-or-later
.section .nro_header,"a"
.global __nro_header_start
__nro_header_start:
    .word 0
    .word _mod_header - __nro_header_start
    .space 0x78
.section .rodata.mod0,"a"
.global _mod_header
_mod_header:
    .ascii "MOD0"
    .word __dynamic_start - _mod_header
    .word __bss_start - _mod_header
    .word __bss_end - _mod_header
    .word 0
    .word 0
    .word __nx_module_runtime - _mod_header
.section .bss.module_runtime,"aw",%nobits
.balign 8
__nx_module_runtime:
    .space 0xd0
