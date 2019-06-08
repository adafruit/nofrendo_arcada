extern "C" {
  #include "emuapi.h"  
  #include "nes_emu.h"
}

#include <Adafruit_Arcada.h>
#include <Audio.h>
#include <AudioStream.h>
#include "display_dma.h"

#if !defined(USE_TINYUSB)
  #error("Please select TinyUSB for the USB stack!")
#endif

Adafruit_Arcada arcada;
Display_DMA tft = Display_DMA();
extern Adafruit_ZeroDMA dma;

#include "AudioPlaySystem.h"

AudioPlaySystem mymixer = AudioPlaySystem();
AudioOutputAnalogStereo  audioOut;
AudioConnection   patchCord1(mymixer, 0, audioOut, 0);
AudioConnection   patchCord2(mymixer, 0, audioOut, 1);

#define SAVEMENU_SELECTIONS 5
#define SAVEMENU_CONTINUE   0
#define SAVEMENU_SAVE       1
#define SAVEMENU_RELOAD     2
#define SAVEMENU_SAVEEXIT   3
#define SAVEMENU_EXIT       4
const char *savemenu_strings[SAVEMENU_SELECTIONS] = {"Continue", "Save", "Reload Save", "Save & Exit", "Exit"};

#if EMU_SCALEDOWN == 1
  // Packed 16-big RGB palette (big-endian order)
  unsigned short palette16[PALETTE_SIZE];
#elif EMU_SCALEDOWN == 2
  // Bizarro palette for 2x2 downsampling.
  // Red and blue values both go into a 32-bit unsigned type, where
  // bits 29:22 are red (8 bits) and bits 18:11 are blue (8 bits):
  // 00RRRRRRRR000BBBBBBBB00000000000
  uint32_t paletteRB[PALETTE_SIZE];
  // Green goes into a 16-bit type, where bits 8:1 are green (8 bits):
  // 0000000GGGGGGGG0
  uint16_t paletteG[PALETTE_SIZE];
  // Later, bitwise shenanigans are used to accumulate 2x2 pixel colors
  // and shift/mask these into a 16-bit result.
#else
  #error("Only scale 1 or 2 supported")
#endif


#define TIMER_LED 13
#define FRAME_LED 12
#define EMUSTEP_LED 11
bool emu_toggle=false;
bool fileSelect=true;

volatile bool test_invert_screen = false;

volatile bool vbl=true;
int skip=0;
uint16_t hold_start_select = 0;
extern uint16_t button_CurState;
char rom_filename_path[512];
    
static void main_step() {
  uint16_t bClick = emu_DebounceLocalKeys();
  
  // Global key handling
  if (bClick & ARCADA_BUTTONMASK_START) {  
    emu_printf("Sta");
  }
  if (bClick & ARCADA_BUTTONMASK_SELECT) {  
    emu_printf("Sel");
  }
  if (bClick & ARCADA_BUTTONMASK_B) {  
    emu_printf("B");
  }
  if (bClick & ARCADA_BUTTONMASK_A) {  
    emu_printf("A");
  }  

  if (button_CurState & (ARCADA_BUTTONMASK_START | ARCADA_BUTTONMASK_SELECT)) {
    hold_start_select++;
    if (hold_start_select == 50) {
      Serial.println("Quit!");
      tft.stop();
      delay(50);
      mymixer.stop();
      uint8_t selected = arcada.menu(savemenu_strings, SAVEMENU_SELECTIONS, ARCADA_WHITE, ARCADA_BLACK);
       
      // save or save+exit
      if ((selected == SAVEMENU_SAVE) || (selected == SAVEMENU_SAVEEXIT)) {
        arcada.fillScreen(ARCADA_BLUE);
        arcada.infoBox("Saving game state...", 0);
        delay(100);
        emu_SaveState();
        arcada.fillScreen(ARCADA_BLACK);
      }

      // reload state
      if (selected == SAVEMENU_RELOAD) {
        arcada.fillScreen(ARCADA_BLUE);
        arcada.infoBox("Loading game state...", 0);
        delay(100);
        emu_LoadState(true);
        arcada.fillScreen(ARCADA_BLACK);
      }
      // save, reload or just continue
      if ((selected == SAVEMENU_SAVE) || (selected == SAVEMENU_CONTINUE) || (selected == SAVEMENU_RELOAD)) {
        tft.refresh();
        mymixer.start();  // keep playing!
      }

      // save+exit or just exit
      if ((selected == SAVEMENU_EXIT) || (selected == SAVEMENU_SAVEEXIT)) {
        nes_End();
        NVIC_SystemReset();
      }
    }
  } else {
    hold_start_select = 0;
  }
  if (fileSelect) {
    while (! arcada.chooseFile("/nes", rom_filename_path, 512, "nes"));
    Serial.print("Selected: "); Serial.println(rom_filename_path);
    arcada.fillScreen(ARCADA_BLACK);
    if (!arcada.getFrameBuffer() && !arcada.createFrameBuffer(EMUDISPLAY_WIDTH, EMUDISPLAY_HEIGHT)) {
      arcada.haltBox("Failed to create framebuffer, out of memory?");
    }
    tft.setFrameBuffer(arcada.getFrameBuffer());     
    fileSelect = false;
    tft.refresh();
    emu_Init(rom_filename_path);
    mymixer.start();
  } else {
    int chan = dma.getChannel();
    uint32_t chctrla = DMAC->Channel[chan].CHCTRLA.reg;
    uint32_t chctrlb = DMAC->Channel[chan].CHCTRLB.reg;
    uint32_t chstat = DMAC->Channel[chan].CHSTATUS.reg;
    uint32_t pend = DMAC->PENDCH.reg;
    uint32_t active = DMAC->ACTIVE.reg;
    // DMA gets 'hung up' sometimes - we can detect and kick it :/
    if (chstat == 3) {
      Serial.printf("DMA pending 0x%08x active 0x%08x\n", pend, active);
      Serial.printf("Channel %d - A=0x%08x B=0x%04x Stat=0x%04x\n", chan, chctrla, chctrlb, chstat);
      Serial.println("Kicking DMA");
      tft.stop();
      mymixer.stop();
      delay(50);
      tft.refresh();
      mymixer.start();
    }
    
    digitalWrite(EMUSTEP_LED, emu_toggle);
    emu_toggle = !emu_toggle;
    emu_Step();
  }
}

static void vblCount() { 
  digitalWrite(TIMER_LED, vbl);
  vbl = !vbl;
#ifdef TIMER_REND
  main_step();
#endif
}

// ****************************************************
// the setup() method runs once, when the sketch starts
// ****************************************************
void setup() {
  // Setup hardware, start USB disk drive
  arcada.begin();
  arcada.filesysBeginMSD();
  delay(100);

  // Wait for serial if desired
  Serial.begin(115200);
  //while (!Serial) delay(10);
  //pinMode(A15, OUTPUT);
  //digitalWrite(A15, HIGH);
  Serial.println("-----------------------------");

  // Init screen with blue so we know its working
  arcada.displayBegin();
  arcada.fillScreen(ARCADA_BLUE);
  arcada.setBacklight(255);

  // Check we have a valid filesys
  if (arcada.filesysBegin()) {
    Serial.println("Found filesystem!");
  } else {
    arcada.haltBox("No filesystem found! For QSPI flash, load CircuitPython. For SD cards, format with FAT");
  }

  // Check we have some ROMs!
  if (!arcada.chdir("/nes")) {
    arcada.haltBox("Cound not find /nes ROMs folder. Please create & add ROMs, then restart.");
  }

  Serial.printf("Filesys & ROM folder initialized, %d files found\n", arcada.filesysListFiles());
  arcada.enableSpeaker(true);
  mymixer.start();

#ifdef TIMER_LED
  pinMode(TIMER_LED, OUTPUT);
#endif
#ifdef FRAME_LED
  pinMode(FRAME_LED, OUTPUT);
#endif
#ifdef EMUSTEP_LED
  pinMode(EMUSTEP_LED, OUTPUT);
#endif

  arcada.timerCallback(200, vblCount);
}


// ****************************************************
// the loop() method runs continuously
// ****************************************************
void loop() {
#ifndef TIMER_REND  
  main_step();
#endif
}



void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b, int index)
{
  if (index<PALETTE_SIZE) {
    //Serial.println("%d: %d %d %d\n", index, r,g,b);
#if EMU_SCALEDOWN == 1
    palette16[index] = __builtin_bswap16(RGBVAL16(r,g,b));
#elif EMU_SCALEDOWN == 2
    // 00RRRRRRRR000BBBBBBBB00000000000
    paletteRB[index] = ((uint32_t)r << 22) | ((uint32_t)b << 11);
    // 0000000GGGGGGGG0
    paletteG[index]  =  (uint16_t)g << 1;
#endif
  }
}

void emu_DrawVsync(void)
{
  volatile boolean vb=vbl;
  skip += 1;
  skip &= VID_FRAME_SKIP;

  while (vbl==vb) {};
}

bool frametoggle;
void emu_DrawLine(unsigned char * VBuf, int width, int height, int line) 
{
  if (line == 0) {
    digitalWrite(FRAME_LED, frametoggle);
    frametoggle = !frametoggle;
  }
#if EMU_SCALEDOWN == 1
  tft.writeLine(width, 1, line, VBuf, palette16);
#elif EMU_SCALEDOWN == 2
  tft.writeLine(width, 1, line, VBuf, paletteRB, paletteG);
#endif
}  

#if 0
// NOT CURRENTLY BEING USED IN NOFRENDO.
// If it does get used, may need to handle 2x2 palette methodology.
// See also writeScreen() in display_dma.cpp.
void emu_DrawScreen(unsigned char * VBuf, int width, int height, int stride) 
{
  if (skip==0) {
    tft.writeScreen(width,height-TFT_VBUFFER_YCROP,stride, VBuf+(TFT_VBUFFER_YCROP/2)*stride, palette16);
  }
}
#endif

int emu_FrameSkip(void)
{
  return skip;
}

void *emu_LineBuffer(int line)
{
  return (void*)tft.getLineBuffer(line);
}


void emu_sndInit() {
  Serial.println("Sound init");  
  AudioMemory(2);
  mymixer.start();
}
