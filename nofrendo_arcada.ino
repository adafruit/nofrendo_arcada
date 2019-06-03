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


#include "AudioPlaySystem.h"

AudioPlaySystem mymixer = AudioPlaySystem();
AudioOutputAnalogStereo  audioOut;
AudioConnection   patchCord1(mymixer, 0, audioOut, 0);
AudioConnection   patchCord2(mymixer, 0, audioOut, 1);


static unsigned char  palette8[PALETTE_SIZE];
static unsigned short palette16[PALETTE_SIZE];

#define TIMER_LED 13
#define FRAME_LED 12
#define EMUSTEP_LED 11
bool emu_toggle=false;
bool fileSelect=true;

volatile bool test_invert_screen = false;

volatile bool vbl=true;
static int skip=0;
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
    if (hold_start_select == 100) {
      Serial.println("Quit!");
      tft.stop();
      delay(50);
      mymixer.stop();
      emu_SaveState();
      nes_End();
      arcada.fillScreen(ARCADA_BLACK);
      fileSelect = true;
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
  emu_init();
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
    palette8[index]  = RGBVAL8(r,g,b);
    palette16[index] = RGBVAL16(r,g,b);
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
  tft.writeLine(width, 1, line, VBuf, palette16);
}  

void emu_DrawScreen(unsigned char * VBuf, int width, int height, int stride) 
{
  if (skip==0) {
    tft.writeScreen(width,height-TFT_VBUFFER_YCROP,stride, VBuf+(TFT_VBUFFER_YCROP/2)*stride, palette16);
  }
}

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
