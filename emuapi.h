#ifndef EMUAPI_H
#define EMUAPI_H
#include <stdint.h>

#define HAS_SND     1
#define CUSTOM_SND  1
//#define TIMER_REND  1

#define DEFAULT_FLASH_ADDRESS 0x2F000  // make sure this is after this programs memory

#define emu_Init(ROM) {nes_Start(ROM); nes_Init(); }
#define emu_Step(x) { nes_Step(); }

#define PALETTE_SIZE         256
#define VID_FRAME_SKIP       0x0
#define TFT_VBUFFER_YCROP    0
#define SINGLELINE_RENDERING 1

#define ACTION_NONE          0
#define ACTION_MAXKBDVAL     12
#define ACTION_EXITKBD       128
#define ACTION_RUNTFT        129
#define ACTION_RUNVGA        130

#ifdef KEYMAP_PRESENT

#define TAREA_W_DEF          32
#define TAREA_H_DEF          32
#define TAREA_END            255
#define TAREA_NEW_ROW        254
#define TAREA_NEW_COL        253
#define TAREA_XY             252
#define TAREA_WH             251

#define KEYBOARD_X           104
#define KEYBOARD_Y           78
#define KEYBOARD_KEY_H       30
#define KEYBOARD_KEY_W       21
#define KEYBOARD_HIT_COLOR   RGBVAL16(0xff,0x00,0x00)

const unsigned short keysw[] = {
  TAREA_XY,KEYBOARD_X,KEYBOARD_Y,
  TAREA_WH,KEYBOARD_KEY_W,KEYBOARD_KEY_H,
  TAREA_NEW_ROW,40,40,
  TAREA_END};
   
const unsigned short keys[] = {
  2,3};  
   
#endif


extern void emu_init(void);
extern void emu_printf(const char *str);
extern void * emu_Malloc(int size);
extern void emu_Free(void * pt);
extern uint8_t *emu_LoadROM(const char *filename);
extern int emu_FileOpen(char * filename);
extern int emu_FileRead(char * buf, int size);
extern unsigned char emu_FileGetc(void);
extern int emu_FileSeek(int seek);
extern void emu_FileClose(void);
extern int emu_FileSize(char * filename);
extern int emu_LoadFile(char * filename, char * buf, int size);
extern int emu_LoadFileSeek(char * filename, char * buf, int size, int seek);
extern void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b, int index);
extern void emu_DrawScreen(unsigned char * VBuf, int width, int height, int stride);
extern void emu_DrawLine(unsigned char * VBuf, int width, int height, int line);
extern void emu_DrawVsync(void);
extern int emu_FrameSkip(void);
extern void * emu_LineBuffer(int line);

extern uint16_t emu_DebounceLocalKeys(void);
extern uint32_t emu_ReadKeys(void);
extern void emu_sndPlaySound(int chan, int volume, int freq);
extern void emu_sndPlayBuzz(int size, int val);
extern void emu_sndInit();
extern void emu_resetus(void);
extern int emu_us(void);

#endif
