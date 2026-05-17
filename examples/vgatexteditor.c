#include <stdint.h>
__attribute__((section(".multiboot")))uint32_t mb[3]={0x1BADB002,0x00010003,-(0x1BADB002+0x00010003)};
volatile uint16_t *vga=(uint16_t*)0xB8000;
char lines[22][79];int lens[22],curLine=0,curCol=0;

static inline uint8_t inb(uint16_t p){uint8_t r;__asm__("inb %1,%0":"=a"(r):"Nd"(p));return r;}

// French AZERTY keyboard scan code to character mapping
static const char map[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    0,'a','z','e','r','t','y','u','i','o','p','^','$', '\n',
    0,'q','s','d','f','g','h','j','k','l','m','%','*',
    0,0,'w','x','c','v','b','n', ',', ';', ':', '!', '\'',
};

void render(){
    for(int y=0;y<25;y++)for(int x=0;x<80;x++)vga[y*80+x]=0x0720;
    const char *hdr="Text Editor";
    for(int k=0;hdr[k];k++)
        vga[0*80+k]=0x0C<<8|hdr[k];
    for(int j=0;j<22;j++){
        int ly=j+1;
        for(int k=0;k<lens[j]&&k<78;k++)
            vga[ly*80+k]=((j==curLine&&k==curCol)?0x70:0x07)<<8|lines[j][k];
        // Fix: display cursor on empty lines or at end of line
        if(j==curLine && curCol<=lens[j] && curCol<78){
            vga[ly*80+curCol]=0x70<<8|' ';
        }
    }
}

void kernel_main(uint32_t m,uint32_t i){
    if(m!=0x2BADB002)while(1);
    
    // Initialize all lines as empty
    for(int j=0;j<22;j++){lens[j]=0;lines[j][0]=0;}
    
    render();
    
    // State machine for escape sequences
    int escState=0; // 0=idle, 1=got ESC, 2=got ESC[
    
    while(1){
        uint8_t s;
        while(!(inb(0x64)&1));s=inb(0x60);
        
        if(s==1)break; // ESC to exit
        
        if(s&0x80)continue; // Key release
        
        int handled=0;
        
        // Handle escape sequences for arrow keys (terminal mode)
        if(escState==0){
            if(s==27){escState=1;handled=1;}
        }else if(escState==1){
            if(s=='['){escState=2;handled=1;}
            else{
                // Not an escape sequence, reset and process normally
                escState=0;
                handled=0;
            }
        }else if(escState==2){
            escState=0;
            if(s=='A'&&curLine>0)curLine--;
            else if(s=='B'&&curLine<21)curLine++;
            else if(s=='C'&&curCol<=lens[curLine])curCol++;  // Fix: <= instead of <
            else if(s=='D'&&curCol>0)curCol--;
            handled=1;
        }
        
        // Also handle raw scan codes for arrow keys (legacy mode)
        if(!handled){
            if(s==0x48&&curLine>0)curLine--;
            else if(s==0x50&&curLine<21)curLine++;
            else if(s==0x4B&&curCol>0)curCol--;
            else if(s==0x4D&&curCol<=lens[curLine])curCol++;  // Fix: <= instead of <
        }
        
        if(handled){render();continue;}
        
        // Handle backspace (scan code 0x0E = 14)
        if(s==0x0E){
            if(curCol>0){
                for(int j=curCol;j<lens[curLine];j++)lines[curLine][j-1]=lines[curLine][j];
                lens[curLine]--;lines[curLine][lens[curLine]]=0;curCol--;
            }else if(curLine>0){
                int pl=lens[curLine-1],cl=lens[curLine];
                for(int j=0;j<=cl;j++)lines[curLine-1][pl+j]=lines[curLine][j];
                lens[curLine-1]+=cl;
                for(int j=curLine;j<21;j++){
                    lens[j]=lens[j+1];
                    for(int k=0;k<=lens[j+1];k++)lines[j][k]=lines[j+1][k];
                }
                curLine--;
            }
        }else if(s==0x1C){ // Enter
            int rem=lens[curLine]-curCol;
            for(int j=21;j>curLine;j--){
                lens[j]=lens[j-1];
                for(int k=0;k<=lens[j-1];k++)lines[j][k]=lines[j-1][k];
            }
            if(rem>0)for(int j=0;j<rem;j++)lines[curLine+1][j]=lines[curLine][curCol+j];
            lens[curLine+1]=rem;lines[curLine+1][rem]=0;
            lens[curLine]=curCol;lines[curLine][curCol]=0;
            curLine++;curCol=0;
        }else if(s<128&&map[s]){
            char c=map[s];
            if(curCol<78){
                for(int j=lens[curLine]+1;j>curCol;j--)lines[curLine][j]=lines[curLine][j-1];
                lines[curLine][curCol]=c;lens[curLine]++;curCol++;
            }
        }
        render();
    }
}
