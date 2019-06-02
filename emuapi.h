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
