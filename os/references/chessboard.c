#include <stdint.h>
__attribute__((section(".multiboot")))static uint32_t h[]={0x1BADB002,0x00010003,-(0x1BADB002+0x1BADB002)};
volatile uint8_t *fb;int pitch=2560;

void dp(int cx,int cy,uint32_t c){
    int r=18,dy,dx,px,py;
    for(dy=-r;dy<=r;dy++){
        for(dx=-r;dx<=r;dx++){
            if(dx*dx+dy*dy<=r*r){
                px=cx+dx;py=cy+dy;
                if(px>=0&&px<640&&py>=0&&py<480){
                    int idx=(py)*pitch+(px)*4;
                    fb[idx]=c&0xFF;
                    fb[idx+1]=(c>>8)&0xFF;
                    fb[idx+2]=(c>>16)&0xFF;
                }
            }
        }
    }
}

void draw_filled_rect(int x0,int y0,int w,int h,uint32_t c){
    int x,y;
    for(y=y0;y<y0+h;y++){
        for(x=x0;x<x0+w;x++){
            if(x>=0&&x<640&&y>=0&&y<480){
                int idx=(y)*pitch+(x)*4;
                fb[idx]=c&0xFF;
                fb[idx+1]=(c>>8)&0xFF;
                fb[idx+2]=(c>>16)&0xFF;
            }
        }
    }
}

void draw_piece(int cx,int cy,uint32_t color,int type){
    // Base: wide rectangle at bottom (like a piece base)
    draw_filled_rect(cx-18,cy-5,36,10,color);
    
    if(type==0){ // Pawn - sphere on top of base
        dp(cx,cy-20,color);
    } else if(type==1){ // Rook - tower with battlements
        draw_filled_rect(cx-14,cy-35,28,30,color);
        draw_filled_rect(cx-18,cy-40,36,6,color);
        draw_filled_rect(cx-14,cy-46,10,7,color);
        draw_filled_rect(cx+4,cy-46,10,7,color);
    } else if(type==2){ // Knight - horse head shape
        draw_filled_rect(cx-12,cy-35,24,30,color);
        dp(cx-8,cy-42,color);
        dp(cx+8,cy-42,color);
        draw_filled_rect(cx-6,cy-48,12,7,color);
    } else if(type==3){ // Bishop - mitre (pointed top)
        draw_filled_rect(cx-10,cy-35,20,30,color);
        draw_filled_rect(cx-6,cy-48,12,14,color);
        dp(cx,cy-52,color);
    } else if(type==4){ // Queen - crown with points
        draw_filled_rect(cx-12,cy-35,24,30,color);
        draw_filled_rect(cx-16,cy-40,32,6,color);
        dp(cx-12,cy-48,color);
        dp(cx+12,cy-48,color);
        dp(cx,cy-52,color);
    } else if(type==5){ // King - cross on top
        draw_filled_rect(cx-12,cy-35,24,30,color);
        draw_filled_rect(cx-14,cy-40,28,6,color);
        draw_filled_rect(cx-4,cy-52,8,14,color);
        draw_filled_rect(cx-12,cy-50,24,6,color);
    }
}

void kernel_main(uint32_t m,uint32_t i){
    int y,x,px,py,dx,dy,ox,oy,sq=55;
    uint32_t lt=0xF0D9B3,dk=0xB58863,pw=0xFFFFFF,pb=0x1A1A1A,c;
    
    if(m!=0x1BADB002&&m!=0x2BADB002)while(1)__asm__("hlt");
    
    uint8_t vbe_func=0x4F;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_func),"d"((uint16_t)0x01CF));
    uint8_t vbe_sub=0x02;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_sub),"d"((uint16_t)0x01CE));
    
    vbe_func=0x4F;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_func),"d"((uint16_t)0x01CF));
    vbe_sub=0x15;
    __asm__ __volatile__("outb %0,%1" : : "a"(vbe_sub),"d"((uint16_t)0x01CE));
    
    fb=(volatile uint8_t*)0xE0000000;int w=640,h=480;
    
    for(y=0;y<h*pitch;y++)fb[y]=0x00;
    
    int board_size=sq*8;
    ox=(640-board_size)/2;
    oy=(480-board_size)/2;
    
    // Draw checkerboard
    for(y=0;y<8;y++){
        for(x=0;x<8;x++){
            px=ox+x*sq;py=oy+y*sq;
            c=((x+y)%2)?dk:lt;
            draw_filled_rect(px,py,sq,sq,c);
        }
    }
    
    // Piece types: 0=pawn, 1=rook, 2=knight, 3=bishop, 4=queen, 5=king
    int board[8][8]={
        {1,2,3,4,5,3,2,1}, // Row 0 (back rank black)
        {0,0,0,0,0,0,0,0}, // Row 1 (pawns black)
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0}, // Row 6 (pawns white)
        {1,2,3,4,5,3,2,1}  // Row 7 (back rank white)
    };
    
    // Draw black pieces (rows 0-1)
    for(x=0;x<8;x++){
        draw_piece(ox+x*sq+sq/2,oy+sq/2,pb,board[0][x]);
        draw_piece(ox+x*sq+sq/2,oy+sq+sq/2,pb,0); // pawns
    }
    
    // Draw white pieces (rows 6-7)
    for(x=0;x<8;x++){
        draw_piece(ox+x*sq+sq/2,oy+6*sq+sq/2,pw,0); // pawns
        draw_piece(ox+x*sq+sq/2,oy+7*sq+sq/2,pw,board[7][x]);
    }
}