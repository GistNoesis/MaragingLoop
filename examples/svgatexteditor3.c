#include <stdint.h>
#define W 640
#define H 480
#define PP 2560

static volatile uint32_t *fb;
char txt[16][80];
int lnlen[16], ccol=0, curl=0;

void pset(int x, int y, uint32_t c) {
    if (x >= 0 && x < W && y >= 0 && y < H)
        fb[y * PP / 4 + x] = c;
}

/* 5x7 block font - row-major, MSB=left pixel */
const uint8_t fs[7] = {0};
const uint8_t fA[7] = {0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11};
const uint8_t fB[7] = {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x1F};
const uint8_t fC[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
const uint8_t fD[7] = {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F};
const uint8_t fE[7] = {0x1F, 0x11, 0x10, 0x1E, 0x10, 0x11, 0x1F};
const uint8_t fF[7] = {0x1F, 0x11, 0x10, 0x1E, 0x10, 0x10, 0x10};
const uint8_t fG[7] = {0x0E, 0x11, 0x10, 0x1F, 0x11, 0x11, 0x0C};
const uint8_t fH[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
const uint8_t fI[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
const uint8_t fJ[7] = {0x1F, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E};
const uint8_t fK[7] = {0x11, 0x11, 0x12, 0x14, 0x16, 0x13, 0x11};
const uint8_t fL[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x1F};
const uint8_t fM[7] = {0x11, 0x1B, 0x15, 0x15, 0x15, 0x11, 0x11};
const uint8_t fN[7] = {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
const uint8_t fO[7] = {0x0C, 0x13, 0x13, 0x13, 0x13, 0x13, 0x0C};
const uint8_t fP[7] = {0x1F, 0x11, 0x11, 0x1F, 0x10, 0x10, 0x10};
const uint8_t fQ[7] = {0x0C, 0x13, 0x13, 0x13, 0x19, 0x13, 0x0A};
const uint8_t fR[7] = {0x1F, 0x11, 0x11, 0x1F, 0x12, 0x14, 0x13};
const uint8_t fS[7] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E};
const uint8_t fT[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
const uint8_t fU[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
const uint8_t fV[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
const uint8_t fW[7] = {0x11, 0x11, 0x15, 0x15, 0x1D, 0x1B, 0x11};
const uint8_t fX[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
const uint8_t fY[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
const uint8_t fZ[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
const uint8_t f0[7] = {0x0C, 0x13, 0x15, 0x19, 0x11, 0x13, 0x0E};
const uint8_t f1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
const uint8_t f2[7] = {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F};
const uint8_t f3[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
const uint8_t f4[7] = {0x02, 0x03, 0x04, 0x08, 0x1F, 0x08, 0x08};
const uint8_t f5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
const uint8_t f6[7] = {0x0E, 0x11, 0x10, 0x1E, 0x11, 0x11, 0x0E};
const uint8_t f7[7] = {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04};
const uint8_t f8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
const uint8_t f9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};

const uint8_t* get_font(char c) {
    if (c == 'A') return fA; if (c == 'B') return fB; if (c == 'C') return fC;
    if (c == 'D') return fD; if (c == 'E') return fE; if (c == 'F') return fF;
    if (c == 'G') return fG; if (c == 'H') return fH; if (c == 'I') return fI;
    if (c == 'J') return fJ; if (c == 'K') return fK; if (c == 'L') return fL;
    if (c == 'M') return fM; if (c == 'N') return fN; if (c == 'O') return fO;
    if (c == 'P') return fP; if (c == 'Q') return fQ; if (c == 'R') return fR;
    if (c == 'S') return fS; if (c == 'T') return fT; if (c == 'U') return fU;
    if (c == 'V') return fV; if (c == 'W') return fW; if (c == 'X') return fX;
    if (c == 'Y') return fY; if (c == 'Z') return fZ;
    if (c == '0') return f0; if (c == '1') return f1; if (c == '2') return f2;
    if (c == '3') return f3; if (c == '4') return f4; if (c == '5') return f5;
    if (c == '6') return f6; if (c == '7') return f7; if (c == '8') return f8;
    if (c == '9') return f9;
    return fs;
}

void dch(int sx, int sy, const uint8_t* f, uint32_t fg) {
    for (int i = 0; i < 7; i++) {
        uint8_t row = f[i];
        int py = sy + i * 3;
        for (int j = 0; j < 5; j++)
            if (row & (1 << (4 - j))) {
                int px = sx + j * 2;
                for (int y = 0; y < 3; y++)
                    for (int x = 0; x < 2; x++)
                        pset(px + x, py + y, fg);
            }
    }
}

void dstr(int sx, int sy, char* s, uint32_t c) {
    while (*s) { dch(sx, sy, get_font(*s), c); sx += 12; s++; }
}

void circ(int cx, int cy, int r, uint32_t c) {
    for (int y = cy - r; y <= cy + r; y++) {
        if (y < 0 || y >= H) continue;
        for (int x = cx - r; x <= cx + r; x++) {
            if (x < 0 || x >= W) continue;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r) pset(x, y, c);
        }
    }
}

void redr() {
    for (int i = 0; i < W * PP / 4; i++) fb[i] = 0x1A1A2E;
    circ(W / 2, H / 2 + 50, 140, 0x00FF00);
    dstr(180, 10, "SVGA EDITOR", 0x00FF55);
    int edy = 60;
    for (int x = 20; x < W - 20; x++) {
        fb[(edy - 3) * PP / 4 + x] = 0x5555BB;
        fb[(edy + 7 * 3 + 10) * PP / 4 + x] = 0x5555BB;
    }
    for (int y = edy - 3; y < edy + 7 * 3 + 10; y++) {
        fb[y * PP / 4 + 20] = 0x5555BB;
        fb[y * PP / 4 + W - 20] = 0x5555BB;
    }
    for (int j = 0; j < 16; j++)
        if (txt[j][0]) dstr(30, edy + j * 7 * 3 + 15, txt[j], 0xDDCCAA);
    if (curl < 16) {
        int ly = edy + curl * 7 * 3;
        for (int y = ly; y < ly + 21 && y < H; y++)
            for (int x = 30 + ccol * 12; x < 30 + ccol * 12 + 10 && x < W; x++)
                fb[y * PP / 4 + x] = 0x5566FF;
    }
}

void hk(uint8_t s) {
    if (s & 0x80) return; /* key release */
    char km[128] = {0};
    km[0x10] = 'q'; km[0x11] = 'w'; km[0x12] = 'e'; km[0x13] = 'r'; km[0x14] = 't'; km[0x15] = 'y';
    km[0x16] = 'u'; km[0x17] = 'i'; km[0x18] = 'o'; km[0x19] = 'p';
    km[0x1E] = 'a'; km[0x1F] = 's'; km[0x20] = 'd'; km[0x21] = 'f'; km[0x22] = 'g';
    km[0x23] = 'h'; km[0x24] = 'j'; km[0x25] = 'k'; km[0x26] = 'l';
    km[0x2C] = 'z'; km[0x2D] = 'x'; km[0x2E] = 'c'; km[0x2F] = 'v'; km[0x30] = 'b';
    km[0x31] = 'n'; km[0x32] = 'm';
    /* Enter key is scancode 0x1C in PS/2 set 1 */
    km[0x1C] = '\n';
    char c = km[s];
    if (c == '\b' || c == 8) {
        if (ccol > 0) {
            for (int i = ccol - 1; i < lnlen[curl]; i++) txt[curl][i] = txt[curl][i + 1];
            txt[curl][--lnlen[curl]] = 0; ccol--; redr();
        }
    } else if (c == '\n' || c == 13) {
        /* Newline: move remaining text to next line */
        int rem = lnlen[curl] - ccol;
        if (curl < 15) {
            for (int i = 0; i <= rem; i++) txt[curl + 1][i] = txt[curl][ccol + i];
            txt[curl][ccol] = 0; lnlen[curl] = ccol;
            curl++; ccol = 0; lnlen[curl] = rem;
        }
        redr(); return;
    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        /* Convert to uppercase for font lookup */
        char u = (c >= 'a' && c <= 'z') ? c - 32 : c;
        if (lnlen[curl] < 79) {
            for (int i = lnlen[curl] + 1; i > ccol; i--) txt[curl][i] = txt[curl][i - 1];
            txt[curl][ccol] = u; lnlen[curl]++; ccol++;
        }
        redr(); return;
    } else if (c == ' ') {
        if (lnlen[curl] < 79) {
            for (int i = lnlen[curl] + 1; i > ccol; i--) txt[curl][i] = txt[curl][i - 1];
            txt[curl][ccol] = ' '; lnlen[curl]++; ccol++; redr();
        }
    }
}

void outb(uint8_t v, uint16_t p) { __asm__("outb %0,%1"::"a"(v), "Nd"(p)); }
uint8_t inb(uint16_t p) {
    uint8_t r;
    __asm__("inb %1,%0" : "=a"(r) : "Nd"(p));
    return r;
}

void kernel_main(uint32_t m, uint32_t ip) {
    uint8_t vf;
    if (m != 0x1BADB002 && m != 0x2BADB002) while (1) __asm__("hlt");
    /* SVGA mode: 640x480x32 */
    vf = 0x4F; outb(vf, 0x01CF);
    vf = 0x15; outb(vf, 0x01CE);
    fb = (volatile uint32_t*)0xE0000000;
    for (int i = 0; i < 16; i++) { txt[i][0] = 0; lnlen[i] = 0; }
    ccol = 0; curl = 0; redr();
    while (1) {
        while (!(inb(0x64) & 1)); hk(inb(0x60));
    }
}