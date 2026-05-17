#include <stdint.h>
__attribute__((section(".multiboot")))
static uint32_t mb_header[] = {0x1BADB002, 0x00010003, -(0x1BADB002+0x00010003)};

void kernel_main(uint32_t magic, uint32_t info_ptr) {
    if (magic != 0x1BADB002 && magic != 0x2BADB002) while(1) __asm__("hlt");

    // Set VBE display mode 0x0115 (640x480x32bpp)
    uint8_t vbe_func = 0x4F;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_func), "d"((uint16_t)0x01CF));
    vbe_func = 0x15;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_func), "d"((uint16_t)0x01CE));

    volatile uint8_t *fb = (volatile uint8_t*)0xE0000000;
    int pitch = 2560, w = 640, h = 480;

    // Clear screen to black
    for(int i = 0; i < h * pitch; i++) fb[i] = 0x00;

    // Circle position (center)
    int cx = w / 2, cy = h / 2, r = 15;

    // Draw circle helper
    void draw_circle(int x, int y, int radius, uint32_t color) {
        for(int py = y - radius; py <= y + radius; py++) {
            if(py < 0 || py >= h) continue;
            for(int px = x - radius; px <= x + radius; px++) {
                if(px < 0 || px >= w) continue;
                int dx = px - x, dy = py - y;
                if(dx * dx + dy * dy <= radius * radius) {
                    uint32_t *pp = (uint32_t*)(fb + py * pitch + px * 4);
                    *pp = color;
                }
            }
        }
    }

    // Initial draw
    draw_circle(cx, cy, r, 0x00FF00);

    // Keyboard input loop
    while(1) {
        // Wait for keyboard input (port 0x64 bit 0 = input buffer full)
        uint8_t status;
        __asm__ __volatile__("inb %1, %0" : "=a"(status) : "d"((uint16_t)0x64));
        if(!(status & 1)) continue;

        // Read scancode from port 0x60
        uint8_t scancode;
        __asm__ __volatile__("inb %1, %0" : "=a"(scancode) : "d"((uint16_t)0x60));

        // Only process make codes (not break codes - bit 7 = 0)
        if(scancode & 0x80) continue;

        // Clear old circle position
        draw_circle(cx, cy, r, 0x000000);

        // Move based on arrow key scancodes
        switch(scancode) {
            case 0x48: cy -= 10; break;  // Up
            case 0x50: cy += 10; break;  // Down
            case 0x4B: cx -= 10; break;  // Left
            case 0x4D: cx += 10; break;  // Right
        }

        // Clamp to screen bounds
        if(cx < r) cx = r;
        if(cx > w - r) cx = w - r;
        if(cy < r) cy = r;
        if(cy > h - r) cy = h - r;

        // Draw new circle position
        draw_circle(cx, cy, r, 0x00FF00);
    }
}