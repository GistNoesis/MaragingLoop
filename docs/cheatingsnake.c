#include <stdint.h>

// External I/O functions
static inline void outb(uint16_t port, uint8_t val) { 
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); 
}

static inline uint8_t inb(uint16_t port) { 
    uint8_t ret; 
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port)); 
    return ret;
}

// VGA Text Mode - framebuffer at 0xB8000
#define VGA_ADDRESS 0xB8000

typedef struct {
    char ascii;
    uint8_t attribute;
} vga_cell_t;

static vga_cell_t *vga_buffer = (vga_cell_t *)VGA_ADDRESS;

// Snake game - press arrow keys or WASD to move step by step
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

// Colors
#define COLOR_BLACK   0x00
#define COLOR_GREEN   0x0A
#define COLOR_RED     0x04
#define COLOR_BLUE    0x1F
#define COLOR_WHITE   0x07

// Snake state
typedef struct {
    int x[100];
    int y[100];
    int length;
    int direction; // 0=up, 1=right, 2=down, 3=left
} snake_t;

// Food position
typedef struct {
    int x;
    int y;
} food_t;

// Game state
static snake_t player;
static food_t apple;
static int direction = 1; // right
static int score = 0;
static int game_over = 0;

// Clear entire screen to black
void clear_screen() {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        vga_buffer[i].ascii = ' ';
        vga_buffer[i].attribute = COLOR_BLACK;
    }
}

// Set single cell with character and color
void set_cell(int x, int y, char ch, uint8_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        vga_buffer[y * SCREEN_WIDTH + x].ascii = ch;
        vga_buffer[y * SCREEN_WIDTH + x].attribute = color;
    }
}

// Draw border once
void draw_border() {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        set_cell(x, 0, '=', COLOR_WHITE);
        set_cell(x, SCREEN_HEIGHT - 1, '=', COLOR_WHITE);
    }
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        set_cell(0, y, '|', COLOR_WHITE);
        set_cell(SCREEN_WIDTH - 1, y, '|', COLOR_WHITE);
    }
}

// Draw score at top center
void draw_score() {
    char score_str[16];
    int pos = 0;
    score_str[pos++] = 'S';
    score_str[pos++] = 'c';
    score_str[pos++] = 'o';
    score_str[pos++] = 'r';
    score_str[pos++] = 'e';
    score_str[pos++] = ':';
    if (score < 10) {
        score_str[pos++] = '0' + score;
    } else {
        score_str[pos++] = '0' + (score / 10);
        score_str[pos++] = '0' + (score % 10);
    }
    score_str[pos] = '\0';
    
    int score_x = (SCREEN_WIDTH - pos) / 2;
    for (int i = 0; score_str[i]; i++) {
        set_cell(score_x + i, 1, score_str[i], COLOR_WHITE);
    }
}

// Draw food (blue 'o')
void draw_food() {
    set_cell(apple.x, apple.y, 'o', COLOR_BLUE);
}

// Render full game state - call once at start and after reset
void render() {
    clear_screen();
    draw_border();
    draw_score();
    draw_food();
    
    // Draw snake: body first (green #), then head (red @)
    for (int i = player.length - 1; i >= 0; i--) {
        char ch = (i == 0) ? '@' : '#';
        uint8_t color = (i == 0) ? COLOR_RED : COLOR_GREEN;
        set_cell(player.x[i], player.y[i], ch, color);
    }
}

// Simple random number generator
static uint32_t rand_state = 12345;
uint32_t my_rand(uint32_t max) {
    if (max == 0) return 0;
    rand_state = rand_state * 1103515245 + 12345;
    return ((rand_state >> 16) & 0x7FFF) % max;
}

// Place food randomly on empty cell
void place_food() {
    int valid = 0;
    do {
        apple.x = (int)(my_rand(SCREEN_WIDTH - 2)) + 1;
        apple.y = (int)(my_rand(SCREEN_HEIGHT - 2)) + 1;
        valid = 1;
        for (int i = 0; i < player.length; i++) {
            if (player.x[i] == apple.x && player.y[i] == apple.y) {
                valid = 0;
                break;
            }
        }
    } while (!valid);
}

// Initialize game state
void init_game() {
    rand_state = 42;
    
    int start_x = SCREEN_WIDTH / 2;
    int start_y = SCREEN_HEIGHT / 2;
    
    player.length = 3;
    direction = 1; // right
    
    for (int i = 0; i < player.length; i++) {
        player.x[i] = start_x - i;
        player.y[i] = start_y;
    }
    
    // Place food immediately to the RIGHT of head so first move eats it
    apple.x = player.x[0] + 1;
    apple.y = player.y[0];
    
    score = 0;
    game_over = 0;
}

// Check if keyboard has scancode ready
static int kb_ready() {
    return (inb(0x64) & 1);
}

// Get key direction from scancode, returns -1 if none/released/non-game key
int get_direction() {
    if (!kb_ready()) return -1;
    
    uint8_t sc = inb(0x60);
    
    // Ignore release codes (high bit set)
    if (sc & 0x80) return -1;
    
    switch(sc) {
        case 0x48: return 0; // UP arrow
        case 0x4D: return 1; // RIGHT arrow  
        case 0x50: return 2; // DOWN arrow
        case 0x4B: return 3; // LEFT arrow
        default: return -1;
    }
}

// Try to move snake one step. Returns 1 if valid, 0 if hit wall/self (game over)
int try_move(int new_dir) {
    // Can't reverse direction (can't go opposite of current)
    if (new_dir == ((direction + 2) % 4)) {
        return 0; // ignore invalid direction change
    }
    
    direction = new_dir;
    
    int new_x = player.x[0];
    int new_y = player.y[0];
    
    switch (direction) {
        case 0: new_y--; break; // UP
        case 1: new_x++; break; // RIGHT
        case 2: new_y++; break; // DOWN  
        case 3: new_x--; break; // LEFT
    }
    
    // Check wall collision (border is at x=0, x=79, y=0, y=24)
    if (new_x < 1 || new_x >= SCREEN_WIDTH - 1 || 
        new_y < 1 || new_y >= SCREEN_HEIGHT - 1) {
        game_over = 1;
        return 0;
    }
    
    // Check self collision (skip checking head position against itself)
    for (int i = 1; i < player.length; i++) {
        if (player.x[i] == new_x && player.y[i] == new_y) {
            game_over = 1;
            return 0;
        }
    }
    
    // Check if ate food
    int ate_food = (new_x == apple.x && new_y == apple.y);
    
    // Move: shift body backwards (tail follows head)
    // If didn't eat, remove tail; if ate, extend by keeping tail and adding new length
    
    if (!ate_food) {
        // Remove last segment (tail), shift all others forward
        for (int i = player.length - 1; i > 0; i--) {
            player.x[i] = player.x[i-1];
            player.y[i] = player.y[i-1];
        }
    } else {
        // Ate food: shift body, add new segment at tail (same as old tail pos)
        for (int i = player.length; i > 0; i--) {
            player.x[i] = player.x[i-1];
            player.y[i] = player.y[i-1];
        }
        player.length++;
    }
    
    // Set new head position
    player.x[0] = new_x;
    player.y[0] = new_y;
    
    if (ate_food) {
        score++;
        // Place food directly in front of the snake's new head position
        switch (direction) {
            case 0: apple.x = new_x;     apple.y = new_y - 1; break; // UP
            case 1: apple.x = new_x + 1; apple.y = new_y;     break; // RIGHT  
            case 2: apple.x = new_x;     apple.y = new_y + 1; break; // DOWN
            case 3: apple.x = new_x - 1; apple.y = new_y;     break; // LEFT
        }
    }
    
    return 1;
}

// Main entry point
void kernel_main(uint32_t magic, uint32_t info_ptr) {
    // Initialize and render initial state
    init_game();
    render();
    
    while (1) {
        // Wait for key press
        int sc = get_direction();
        
        if (sc >= 0 && sc <= 3) {
            // Game over - reset on any key
            if (game_over) {
                init_game();
                render();
                continue;
            }
            
            // Try to move
            if (try_move(sc)) {
                // Only refresh the cells that changed:
                // Clear old tail position
                int old_tail_x = player.x[player.length - 1];
                int old_tail_y = player.y[player.length - 1];
                set_cell(old_tail_x, old_tail_y, ' ', COLOR_BLACK);
                
                // If we ate food, draw new food; if not, head already drawn by move
                // Actually head is already drawn in try_move... wait no, 
                // render() draws everything. Let's just clear and redraw minimal:
                
                // Clear old tail (if didn't eat) or leave it (if ate, new tail was added)
                // The tail moved from position A to B, so we need to erase old tail
            }
            
            // Render full frame after each move
            render();
        }
    }
}