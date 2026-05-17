#include <stdint.h>
__attribute__((section(".multiboot")))
static uint32_t mb_header[] = {0x1BADB002, 0x00010003, -(0x1BADB002+0x00010003)};

void kernel_main(uint32_t magic,uint32_t info_ptr){
if(magic!=0x1BADB002&&magic!=0x2BADB002)while(1)__asm__("hlt");

// Set VBE display mode 0x0115 (640x480x32bpp) via VBE controller ports
uint8_t vbe_func=0x4F;
__asm__ __volatile__("outb %0,%1" : : "a"(vbe_func),"d"((uint16_t)0x01CF));
uint8_t vbe_sub=0x02;
__asm__ __volatile__("outb %0,%1" : : "a"(vbe_sub),"d"((uint16_t)0x01CE));

// Set mode 0x0115
vbe_func=0x4F;
__asm__ __volatile__("outb %0,%1" : : "a"(vbe_func),"d"((uint16_t)0x01CF));
vbe_sub=0x15;
__asm__ __volatile__("outb %0,%1" : : "a"(vbe_sub),"d"((uint16_t)0x01CE));

// Framebuffer at 0xE0000000, pitch=2560 (640*4 bytes per row), 32bpp BGR
volatile uint8_t *fb=(volatile uint8_t*)0xE0000000;
int pitch=2560;
int w=640,h=480;

// Clear screen to black (BGR: 0x000000)
for(int i=0;i<h*pitch;i++)fb[i]=0x00;

// Draw a large green circle (BGR: Blue=0, Green=255, Red=0 = 0x00FF00)
int cx=w/2,cy=h/2,r=180;
for(int py=cy-r;py<=cy+r;py++){
if(py<0||py>=h)continue;
for(int px=cx-r;px<=cx+r;px++){
if(px<0||px>=w)continue;
int dx=px-cx,dy=py-cy;
if(dx*dx+dy*dy<=r*r){
uint32_t *pp=(uint32_t*)(fb+py*pitch+px*4);
*pp=0x00FF00;}}}

while(1)__asm__("hlt");}