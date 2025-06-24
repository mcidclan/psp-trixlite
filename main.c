#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psprtc.h>

#define BOARD_X 152
#define BOARD_Y 4
#define NBR_ROWS 24
#define NBR_PIECES 5
#define NBR_PIECES_LIST 20
#define BLOCK_SIZE 11
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define HORIZONTAL_LENGTH 512
#define HORIZONTAL_BYTES_LENGTH 2048

PSP_MODULE_INFO("trixlite", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-1024);

void sceDmacMemcpy(void *dst, const void *src, int size);
int atexit(void(*function)(void)) {
    function();
    return 0;
}
void exit(int status) {
    sceKernelExitGame();
    while(1);
}

typedef struct {
    u64 data;
    u32 color;
} Piece;

typedef struct {
    u64 data;
    char pos;
} MovablePiece;

static u16 __attribute__((aligned(8))) rows[NBR_ROWS] = {0};

static Piece __attribute__((aligned(8))) pieces[NBR_PIECES_LIST] = {
    {0x080008000800080, 0xFFFF0000},
    {0x080038000000000, 0xFF00FF00},
    {0x1C0008000000000, 0xFF0000FF},
    {0x08000C000400000, 0xFF00FFFF},
    {0x180018000000000, 0xFF0080FF},
    //
    {0x3C0000000000000, 0xFFFF0000},
    {0x180008000800000, 0xFF00FF00},
    {0x08000C000800000, 0xFF0000FF},
    {0x0C0018000000000, 0xFF00FFFF},
    {0x180018000000000, 0xFF0080FF},
    //
    {0x080008000800080, 0xFFFF0000},
    {0x380020000000000, 0xFF00FF00},
    {0x08001C000000000, 0xFF0000FF},
    {0x08000C000400000, 0xFF00FFFF},
    {0x180018000000000, 0xFF0080FF},
    //
    {0x3C0000000000000, 0xFFFF0000},
    {0x200020003000000, 0xFF00FF00},
    {0x080018000800000, 0xFF0000FF},
    {0x0C0018000000000, 0xFF00FFFF},
    {0x180018000000000, 0xFF0080FF}
};

static u32 const vram[3] = {
    0x44000000,
    0x44088000,
    0x44110000
};

void drawBlock(const u8 vid, const u8 x, const u8 y,
  const u32 color, const u16 xdisp, const u16 ydisp) {
    u32* const display = (u32*)vram[vid];
    u8 i = BLOCK_SIZE;
    while (--i) {
        u8 j = BLOCK_SIZE;
        while (--j) {
            display[(i+x*BLOCK_SIZE+xdisp) | ((j+y*BLOCK_SIZE+ydisp) << 9)] = color;
        }
    }
}

void drawRow(const u8 vid, const u16 value, const u8 y, const u32 color) {
    u8 x = 16;
    while (x--) {
        if (value & (0x8000 >> x)) {
            drawBlock(vid, x, y, color, BOARD_X, BOARD_Y);
        }
    }
}

void draw4Rows(const u8 vid, const u64 value, u8 y, const u32 color) {
    u8 x = 64;
    while (x--) {
        const u8 _x = (x % 16);
        if (!_x) {
            y++;
        }
        if (value & (0x8000000000000000 >> x)) {
            drawBlock(vid, _x, y, color, BOARD_X, BOARD_Y);
        }
    }
}

u8 isCollid(u16* const a, const u64 piece) {
    const u64 target = 
      (((u64)a[0]) << 0)  |
      (((u64)a[1]) << 16) |
      (((u64)a[2]) << 32) |
      (((u64)a[3]) << 48);
    return (target | piece) > (target ^ piece);
}

void pieceToRows(u16* const target, const u64 piece) {
    target[3] |= (piece & 0xFFFF000000000000) >> 48;
    target[2] |= (piece & 0x0000FFFF00000000) >> 32;
    target[1] |= (piece & 0x00000000FFFF0000) >> 16;
    target[0] |= (piece & 0x000000000000FFFF);
}

// Drop blocks from above to fill removed lines
void drop(u8 from) {
    const u32 disp = HORIZONTAL_BYTES_LENGTH * BOARD_Y;
    const u32 bytes = HORIZONTAL_BYTES_LENGTH * BLOCK_SIZE;
    const u32 size = from * bytes;
    const u32 vram1 = vram[1] + disp;
    const u32 vram2 = vram[2] + disp;
    sceDmacMemcpy((void*)vram2, (void*)vram1, size);
    sceDmacMemcpy((void*)(vram1 + bytes), (void*)vram2, size);
    do {
        rows[from] = from > 0 ? rows[from - 1] : 0x8001;
    } while (from--);
}

void reset() {
    u8 step = 0;
    const u8 end = NBR_ROWS - 1;
    while (step < end) {
        drop(step++);
    }
}

void removeFullRows(u8 step) {
    const u8 end = step + 4;
    while (step < end) {
        if (rows[step] == 0xFFFF) {
            drop(step);
        }
        step++;
    }
}

int main() {
    SceCtrlData pad;
    sceDisplaySetMode(0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceDisplaySetFrameBuf((void*)vram[0], HORIZONTAL_LENGTH,
    PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

    u64 runtick = 0;
    u8 step = NBR_ROWS, id = 0, pressed = 0, prevStep = -1;
    MovablePiece piece = {0, 0}, initial = {0, 0};

    sceRtcGetCurrentTick(&runtick);
    u64 keytick = runtick;
    const u64 tickres = sceRtcGetTickResolution() / 1000;

    while (step--) {
        rows[step] = (step == NBR_ROWS - 1) ? 0xFFFF : 0x8001;
        drawRow(1, rows[step], step, 0xFF808080);
    }

    do {
        sceDmacMemcpy((void*)vram[0], (void*)vram[1], HORIZONTAL_BYTES_LENGTH * SCREEN_HEIGHT);
        sceCtrlReadBufferPositive(&pad, 1);

        u64 tick = 0;
        sceRtcGetCurrentTick(&tick);

        if (!piece.data) {
            id = ((u8)(tick / 3)) % NBR_PIECES;
            piece.data = pieces[id].data;
            piece.pos = 0;
            initial = piece;
            step = 0;
            if (!prevStep) {
              reset();
            }
        }
        prevStep = step;
        
        draw4Rows(0, piece.data, step, pieces[id].color);
    
        const u16 stepDuration = pad.Buttons & PSP_CTRL_DOWN ? 50 : 500;
        u16* target = &rows[step];
        
        if ((tick - runtick) / tickres >= stepDuration) {
            target = &rows[++step];
            runtick = tick;
        }
        
        if (isCollid(target, piece.data)) {
            pieceToRows((u16*)(target-1), piece.data);
            draw4Rows(1, piece.data, step-1, pieces[id].color);
            removeFullRows(step-1);
            piece.data = 0;
            piece.pos = 0;
        }
        else if ((tick - keytick) / tickres >= 100) {
            const MovablePiece prev = piece;
            
            if (pad.Buttons & PSP_CTRL_LEFT) {
                piece.pos++;
            } else if(pad.Buttons & PSP_CTRL_RIGHT) {
                piece.pos--;
            }
            
            if ((pad.Buttons & PSP_CTRL_SQUARE) || (pad.Buttons & PSP_CTRL_CIRCLE)) {
                if (!pressed) {
                    id = pad.Buttons & PSP_CTRL_SQUARE ?
                        (id + NBR_PIECES) % NBR_PIECES_LIST :
                        (id - NBR_PIECES + NBR_PIECES_LIST) % NBR_PIECES_LIST;
                    initial.data = pieces[id].data;
                    pressed = 1;
                }
            } else pressed = 0;

            piece.data = piece.pos < 0 ?
            initial.data >> -piece.pos : initial.data << piece.pos;
        
            if (isCollid(target, piece.data)) {
                piece = prev;
            }
            
            keytick = tick;
        }
        sceDisplayWaitVblankStart();
    } while (!(pad.Buttons & PSP_CTRL_SELECT));
    return 0;
}
