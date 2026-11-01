/* SPDX-License-Identifier: AGPL-3.0-or-later */
OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
PHDRS {
  text PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
  dynamic PT_DYNAMIC FLAGS(6);
}
SECTIONS {
  . = 0;
  .text : { KEEP(*(.nro_header)) *(.text .text.*) *(.plt .plt.*) } :text
  . = ALIGN(0x1000);
  .rodata : { *(.rodata.mod0) *(.rodata .rodata.*) } :rodata
  .hash : { *(.hash) } :rodata
  .gnu.hash : { *(.gnu.hash) } :rodata
  .dynsym : { *(.dynsym) } :rodata
  .dynstr : { *(.dynstr) } :rodata
  .rela.dyn : { *(.rela.dyn) } :rodata
  .rela.plt : { *(.rela.plt) } :rodata
  .note.gnu.build-id : { *(.note.gnu.build-id) } :rodata
  . = ALIGN(0x1000);
  .data : { *(.data .data.*) *(.data.rel.ro .data.rel.ro.*) } :data
  .got : { *(.got .got.*) } :data
  .dynamic : { HIDDEN(__dynamic_start = .); *(.dynamic) } :data :dynamic
  . = ALIGN(0x1000);
  .bss (NOLOAD) : {
    HIDDEN(__bss_start = .);
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(0x1000);
    HIDDEN(__bss_end = .);
  } :data
  /DISCARD/ : { *(.comment) *(.eh_frame .eh_frame_hdr) *(.interp) *(.note.GNU-stack) }
}
