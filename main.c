#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psprtc.h>

#define NBR_ROWS 24
#define NBR_PIECES 5
#define BLOCK_SIZE 10
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define HORIZONTAL_LENGTH 512
#define HORIZONTAL_BYTES_LENGTH 2048

PSP_MODULE_INFO("tl", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-1024);

void sceDmacMemcpy(void *dst, const void *src, int size);
int atexit(void(*function)(void)) {
    function();
    return 0;
}
void exit(int status) {
    while(1);
}
typedef struct {
    u64 data;
    u32 color;
} Piece;

static u16 __attribute__((aligned(8))) rows[NBR_ROWS] = {0};

static Piece __attribute__((aligned(8))) pieces[NBR_PIECES] = {
    {0x080008000800080, 0xFFFF0000},
    {0x080038000000000, 0xFF00FF00},
    {0x08001C000000000, 0xFF0000FF},
    {0x0C0018000000000, 0xFF00FFFF},
    {0x180018000000000, 0xFF0080FF}
};

static u32* const vram[2] = {
    ((u32*)0x44000000),
    ((u32*)0x44088000)
};

void drawBlock(const u8 vid, const u8 x, const u8 y, const u32 color) {
    u8 i = BLOCK_SIZE;
    while(--i) {
        u8 j = BLOCK_SIZE;
        while(--j) {
            vram[vid][(i+x*BLOCK_SIZE) | ((j+y*BLOCK_SIZE) << 9)] = color;
        }
    }
}

void drawRow(const u8 vid, const u16 value, const u8 y, const u32 color) {
    u8 x = 16;
    while(x--) {
        if(value & (0x8000 >> x)) {
            drawBlock(vid, x, y, color);
        }
    }
}

void draw4Rows(const u8 vid, const u64 value, u8 y, const u32 color) {
    u8 x = 64;
    while(x--) {
        const u8 _x = (x % 16);
        if(!_x) {
            y++;
        }
        if(value & (0x8000000000000000 >> x)) {
            drawBlock(vid, _x, y, color);
        }
    }
}

u8 isCollid(u16* const target, const u64 piece) {
    return (*(u64*)target | piece) > (*(u64*)target ^ piece);
}

void drop(u8 from) {
    const u32 bytes = HORIZONTAL_BYTES_LENGTH * BLOCK_SIZE;
    sceDmacMemcpy((void*)vram[1] + bytes, vram[1], from * bytes);
    do {
        rows[from] = from > 0 ? rows[from - 1] : 0;
    } while(from--);
}

void removeFullRows(u8 step) {
    const u8 end = step + 4;
    while(step < end) {
        if(rows[step] == 0xFFFF) {
            drop(step);
        }
        step++;
    }
}

int main() {
    SceCtrlData pad;
    sceDisplaySetMode(1, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceDisplaySetFrameBuf(vram[0], HORIZONTAL_LENGTH,
    PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);

    u8 step = NBR_ROWS, id = 0;
    u64 piece = 0, runtick = 0;

    sceRtcGetCurrentTick(&runtick);
    const u64 tickres = sceRtcGetTickResolution() / 1000;    

    while(step--) {
        rows[step] = (step == NBR_ROWS - 1) ? 0xFFFF : 0x8001;
        drawRow(1, rows[step], step, 0xFF808080);
    }

    do {
        sceDmacMemcpy(vram[0], vram[1], HORIZONTAL_BYTES_LENGTH * SCREEN_HEIGHT);
        sceCtrlReadBufferPositive(&pad, 1);

        u64 tick = 0;
        sceRtcGetCurrentTick(&tick);

        if(!piece) {
            id = ((u8)(tick / 3)) % 5;
            piece = pieces[id].data;
            step = 0;
        }

        draw4Rows(0, piece, step, pieces[id].color);

        if((tick - runtick) / tickres >= 125) { //< speed
            const u64 prev = piece;
            u16* const target = &rows[++step];
            if(pad.Buttons & PSP_CTRL_LEFT) {
                piece <<= 1;
            } else if(pad.Buttons & PSP_CTRL_RIGHT) {
                piece >>= 1;
            }
            if(isCollid(target, piece)) {
                piece = prev;
            }
            if(isCollid(target, piece)) {
                *(u64*)(target-1) |= piece;
                draw4Rows(1, piece, step-1, pieces[id].color);
                removeFullRows(step-1);
                piece = 0;
            }
            runtick = tick;
        }
        
        sceDisplayWaitVblankStart();
    } while(!(pad.Buttons & PSP_CTRL_SELECT));
    sceKernelExitGame();
    return 0;
}
