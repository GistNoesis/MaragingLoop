# Multiboot header
.section .multiboot, "a", @progbits
    .long 0x1BADB002          # magic
    .long 0x00000003          # flags: request memory info and video mode
    .long -(0x1BADB002 + 0x00000003) # checksum
    .long 0                   # header_addr (optional)
    .long 0                   # load_addr (optional)
    .long 0                   # load_end_addr (optional)
    .long 0                   # bss_end_addr (optional)
    .long 0                   # entry_addr (optional)
    .long 0                   # mode_type (0 = linear framebuffer)
    .long 1024                # width
    .long 768                 # height
    .long 32                  # depth

# Entry point
.section .text
.global _start
_start:
    # The Multiboot spec passes info in registers:
    # eax = magic number
    # ebx = pointer to multiboot_info structure
    
    # Push arguments onto the stack for kernel_main (cdecl convention)
    pushl %ebx
    pushl %eax
    call kernel_main
    addl $8, %esp             # Clean up stack (add 8 to stack pointer)
    
halt:
    hlt
    jmp halt
