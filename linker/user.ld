OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS {

    . = 4096;

    .text : { 
        *(.text.entry)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4K);

    .bss : { 
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }

    .data : { 
        *(.data .data.*) 
        *(.sdata .sdata.*)
    }

    . = ALIGN(4K);
    
    /DISCARD/ : { *(.eh_*) }
}