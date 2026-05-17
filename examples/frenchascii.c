/* ──────────────────────────────────────────────────────────────────────────────
   Keyboard Helpers
   ────────────────────────────────────────────────────────────────────────────── */
static int kb_avail() { return (inb(0x64) & 1); }

/* ──────────────────────────────────────────────────────────────────────────────
   Kernel Main
   ────────────────────────────────────────────────────────────────────────────── */
void kernel_main(uint32_t magic, uint32_t info_ptr) {
    __asm__ volatile("mov %0, %%esp" : : "r"(kernel_stack + sizeof(kernel_stack)) : "memory");
    setup_gdt();
    __asm__ volatile("cli");
    
    for(int i=0; i<4000; i++) vga[i] = 0x0720;
    vga_print_line(2, 25, "Minimal OS (Network Debug)", 0x0A);
    
    low_mem_base = find_free_memory(info_ptr);
    if (low_mem_base == 0) low_mem_base = 0x10000;
    char mem_str[12]; fmt_hex(low_mem_base, mem_str);
    vga_print_line(4, 0, "Low Mem Base:", 0x07); vga_print_line(4, 14, mem_str, 0x0A);

    if (low_mem_base == 0) {
        vga_print_line(5, 2, "FATAL: Memory map failed!", 0x0C);
        while(1) __asm__("hlt");
    }
    
    init_network();
    
    char input[64]; int idx=0; int cur_row = 20;
static const char map[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    0,'a','z','e','r','t','y','u','i','o','p','^','$', '\n',
    0,'q','s','d','f','g','h','j','k','l','m','%','*',
    0,0,'w','x','c','v','b','n', ',', ';', ':', '!', '\'',
};

    while(1) {
        poll_network();
        if(kb_avail()) {
            uint8_t raw = inb(0x60);
            if(raw & 0x80) continue;
            uint8_t sc = raw & 0x7F;
            char c = (sc < sizeof(map)) ? map[sc] : 0;
            
            if(c == '\n') {
                vga_put(cur_row, 20, '\n', 0x07);
                if(idx > 0) {
                    vga_put(cur_row+1, 20, '>', 0x0A);
                    for(int i=0; i<idx; i++) vga_put(cur_row+1, 21+i, input[i], 0x0A);
                    vga_put(cur_row+1, 21+idx, '\n', 0x0A);
                }
                cur_row += 2; if(cur_row > 24) cur_row = 20;
            }
            else if(c == '\b') { if(idx>0) { idx--; vga_put(cur_row, 20+2+idx, ' ', 0x07); vga_put(cur_row, 20+2+idx, '$', 0x0F); } }
            else if(c >= ' ' && c <= '~' && idx < 63) { input[idx++] = c; vga_put(cur_row, 20+2+idx-1, c, 0x0F); }
        }
        __asm__ volatile("pause" ::: "memory");
    }
}
