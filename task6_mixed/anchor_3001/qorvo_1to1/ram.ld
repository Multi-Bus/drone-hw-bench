ENTRY(start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x20004000, LENGTH = 0x1C000
}

SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > RAM
    .data : { *(.data .data.*) } > RAM
    .ARM.exidx : { *(.ARM.exidx*) } > RAM
    .bench (NOLOAD) : {
        KEEP(*(.bench))
    } > RAM
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
        end = .;
    } > RAM
}

ASSERT(__bss_end <= 0x2001E000, "RAM firmware overlaps stack")
