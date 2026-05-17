#include <stdint.h>
__attribute__((section(".multiboot")))
static uint32_t mb_header[] = {0x1BADB002, 0x00010003, -(0x1BADB002+0x00010003)};

static uint32_t rng=12345;
static uint32_t myrand(){
    rng^=(rng<<13);
    rng^=(rng>>17);
    rng^=(rng<<5);
    return rng;
}

void kernel_main(uint32_t magic, uint32_t info_ptr) {
    if (magic != 0x1BADB002 && magic != 0x2BADB002) while(1) __asm__("hlt");

    uint8_t vf=0x4F;
    __asm__ __volatile__("outb %0,%1" : : "a"(vf), "d"((uint16_t)0x01CF));
    vf=0x15;
    __asm__ __volatile__("outb %0,%1" : : "a"(vf), "d"((uint16_t)0x01CE));

    volatile uint8_t *fb=(volatile uint8_t*)0xE0000000;
    int pitch=2560, w=640, h=480;
    for(int i=0;i<h*pitch;i++) fb[i]=0x00;

    void draw_rect(int x,int y,int color){
        for(int py=y;py<y+10&&py<h;py++){
            for(int px=x;px<x+10&&px<w;px++){
                uint32_t *p=(uint32_t*)(fb+py*pitch+px*4);
                *p=color;
            }
        }
    }

    #define GRID 10
    #define MAXLEN 100
    int sx=w/2, sy=h/2;
    int dir=0; // 0=right,1=down,2=left,3=up
    int len=3;
    int segs[MAXLEN][2];
    for(int i=0;i<len;i++){segs[i][0]=sx-i*GRID; segs[i][1]=sy;}

    // Place food on same grid as snake (no GRID/2 offset)
    int fx,fy,ok;
    do {
        fx=(myrand()%((w-GRID)/GRID))*GRID;
        fy=(myrand()%((h-GRID)/GRID))*GRID;
        ok=1;
        for(int i=0;i<len;i++){
            if(segs[i][0]==fx && segs[i][1]==fy){ok=0;break;}
        }
    } while(!ok);

    // Initial draw
    for(int i=0;i<h*pitch;i++) fb[i]=0x00;
    draw_rect(fx, fy, 0x0000FF);
    for(int i=0;i<len;i++){
        int color = (i == 0) ? 0xFF0000 : 0x00FF00; // Red head, Green body
        draw_rect(segs[i][0], segs[i][1], color);
    }

    while(1){
        uint8_t st;
        __asm__ __volatile__("inb %1, %0" : "=a"(st) : "d"((uint16_t)0x64));
        if(!(st&1)) continue;
        uint8_t sc;
        __asm__ __volatile__("inb %1, %0" : "=a"(sc) : "d"((uint16_t)0x60));
        if(sc&0x80) continue;

        switch(sc){
            case 0x4D: if(dir!=2) dir=0; break;
            case 0x50: if(dir!=3) dir=1; break;
            case 0x4B: if(dir!=0) dir=2; break;
            case 0x48: if(dir!=1) dir=3; break;
        }

        int nx=segs[0][0], ny=segs[0][1];
        switch(dir){
            case 0: nx+=GRID; break;
            case 1: ny+=GRID; break;
            case 2: nx-=GRID; break;
            case 3: ny-=GRID; break;
        }

        if(nx<0||nx>=w||ny<0||ny>=h) continue;

        int hit=0;
        for(int i=0;i<len-1;i++){
            if(segs[i][0]==nx && segs[i][1]==ny){hit=1;break;}
        }
        if(hit) continue;

        int ate=0;
        if(nx==fx && ny==fy){
            ate=1;
            len++;
            int ok2;
            do {
                fx=(myrand()%((w-GRID)/GRID))*GRID;
                fy=(myrand()%((h-GRID)/GRID))*GRID;
                ok2=1;
                for(int i=0;i<len;i++){
                    if(segs[i][0]==fx && segs[i][1]==fy){ok2=0;break;}
                }
            } while(!ok2);
        }

        for(int i=len-1;i>0;i--){
            segs[i][0]=segs[i-1][0];
            segs[i][1]=segs[i-1][1];
        }
        segs[0][0]=nx;
        segs[0][1]=ny;

        for(int i=0;i<h*pitch;i++) fb[i]=0x00;
        draw_rect(fx, fy, 0x0000FF);
        for(int i=0;i<len;i++){
            int color = (i == 0) ? 0xFF0000 : 0x00FF00; // Red head, Green body
            draw_rect(segs[i][0], segs[i][1], color);
        }
    }
}