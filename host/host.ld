ENTRY(extension_boot)
SECTIONS {
    . = 0x12497b78;
    .extension : { *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(4); }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
