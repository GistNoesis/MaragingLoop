ENTRY(_start)

SECTIONS
{
    . = 0x100000;                    /* Load kernel at address 0x100000 */

    .multiboot : {
        *(.multiboot)                /* Include the Multiboot header section */
    }

    .text : {
        *(.text)                     /* Include the text section for code */
    }

    .rodata : {
        *(.rodata)                   /* Include read-only data */
    }

    .data : {
        *(.data)                     /* Include initialized data */
    }

    .bss : {
        *(.bss)                      /* Include uninitialized data */
        *(COMMON)
    }
}

