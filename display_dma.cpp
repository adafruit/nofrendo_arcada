/*
  Copyright Frank Bösing, 2017
  GPL license at https://github.com/FrankBoesing/Teensy64
*/

#if defined(__SAMD51__)
#include <Adafruit_Arcada.h>
extern Adafruit_Arcada arcada;
extern volatile bool test_invert_screen;

#include "display_dma.h"
#if defined(USE_SPI_DMA)
  #error("Must not have SPI DMA enabled in Adafruit_SPITFT.h")
#endif

#include <Adafruit_ZeroDMA.h>
#include "wiring_private.h"  // pinPeripheral() function
#include <malloc.h>          // memalign() function

#include "iopins.h"

// Actually 50 MHz due to timer shenanigans below, but SPI lib still thinks it's 24 MHz
#define SPICLOCK 24000000

Adafruit_ZeroDMA dma;                  ///< DMA instance
DmacDescriptor  *dptr         = NULL;  ///< 1st descriptor
DmacDescriptor  *descriptor    = NULL; ///< Allocated descriptor list
int numDescriptors;
int descriptor_bytes;

static uint16_t *screen = NULL;

volatile bool cancelled = false;
volatile uint8_t ntransfer = 0;

Display_DMA *foo; // Pointer into class so callback can access stuff

// DMA transfer-in-progress indicator and callback
static volatile bool dma_busy = false;
static void dma_callback(Adafruit_ZeroDMA *_dma) {
  dma_busy = false;
  foo->dmaFrame();
}

void Display_DMA::dmaFrame(void) {
  ntransfer++;
  if (ntransfer >= SCREEN_DMA_NUM_SETTINGS) {   
    ntransfer = 0;
  }

  digitalWrite(ARCADA_TFT_DC, 0);
  ARCADA_TFT_SPI.transfer(ILI9341_SLPOUT);
  digitalWrite(ARCADA_TFT_DC, 1);
  digitalWrite(ARCADA_TFT_CS, 1);
  ARCADA_TFT_SPI.endTransaction();
  if (cancelled) {
    return;
  }
  setAreaCentered();
  cancelled = false;
  
  ARCADA_TFT_SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(ARCADA_TFT_CS, 0);
  digitalWrite(ARCADA_TFT_DC, 0);
  ARCADA_TFT_SPI.transfer(ILI9341_RAMWR);
  digitalWrite(ARCADA_TFT_DC, 1);

#ifdef HIGH_SPEED_SPI
  int sercom_id_core, sercom_id_slow;
  sercom_id_core = DISPLAY_SERCOM_GCLKIDCORE;
  sercom_id_slow = DISPLAY_SERCOM_GCLKIDSLOW;

  // Override SPI clock source to use 100 MHz peripheral clock (for 50 MHz SPI)
  GCLK_PCHCTRL_Type pchctrl;

  GCLK->PCHCTRL[sercom_id_core].bit.CHEN = 0;     // Disable timer
  while(GCLK->PCHCTRL[sercom_id_core].bit.CHEN);  // Wait for disable
  pchctrl.bit.GEN                        = 2;     // Use GENERIC_CLOCK_GENERATOR_100M defined in startup.c
  pchctrl.bit.CHEN                       = 1;
  GCLK->PCHCTRL[sercom_id_core].reg      = pchctrl.reg;
  while(!GCLK->PCHCTRL[sercom_id_core].bit.CHEN); // Wait for enable

  GCLK->PCHCTRL[sercom_id_slow].bit.CHEN = 0;     // Disable timer
  while(GCLK->PCHCTRL[sercom_id_slow].bit.CHEN);  // Wait for disable
  pchctrl.bit.GEN                        = 2;     // Use GENERIC_CLOCK_GENERATOR_100M defined in startup.c
  pchctrl.bit.CHEN                       = 1;
  GCLK->PCHCTRL[sercom_id_slow].reg      = pchctrl.reg;
  while(!GCLK->PCHCTRL[sercom_id_slow].bit.CHEN); // Wait for enable
#endif

  dma_busy = true;
  dma.startJob(); // Trigger next SPI DMA transfer
}



static bool setDmaStruct() {
  if (dma.allocate() != DMA_STATUS_OK) { // Allocate channel
    Serial.println("Couldn't allocate DMA");
    return false;
  }
  // The DMA library needs to alloc at least one valid descriptor,
  // so we do that here. It's not used in the usual sense though,
  // just before a transfer we copy descriptor[0] to this address.

  descriptor_bytes = EMUDISPLAY_WIDTH * EMUDISPLAY_HEIGHT / 2; // each one is half a screen but 2 bytes per screen so this is correct
  numDescriptors = 4;

  if (NULL == (dptr = dma.addDescriptor(NULL, NULL, descriptor_bytes, DMA_BEAT_SIZE_BYTE, false, false))) {
    Serial.println("Couldn't add descriptor");
    dma.free(); // Deallocate DMA channel
    return false;
  }

  // DMA descriptors MUST be 128-bit (16 byte) aligned.
  // memalign() is considered obsolete but it's replacements
  // (aligned_alloc() or posix_memalign()) are not currently
  // available in the version of ARM GCC in use, but this
  // is, so here we are.
  if (NULL == (descriptor = (DmacDescriptor *)memalign(16, numDescriptors * sizeof(DmacDescriptor)))) {
    Serial.println("Couldn't allocate descriptors");
    return false;
  }
  int                dmac_id;
  volatile uint32_t *data_reg;
  dmac_id  = DISPLAY_SERCOM_DMACID;
  data_reg = &DISPLAY_SERCOM->SPI.DATA.reg;
  dma.setPriority(DMA_PRIORITY_3);
  dma.setTrigger(dmac_id);
  dma.setAction(DMA_TRIGGER_ACTON_BEAT);

  // Initialize descriptor list.
  for(int d=0; d<numDescriptors; d++) {
    descriptor[d].BTCTRL.bit.VALID    = true;
    descriptor[d].BTCTRL.bit.EVOSEL   =
      DMA_EVENT_OUTPUT_DISABLE;
    descriptor[d].BTCTRL.bit.BLOCKACT =
       DMA_BLOCK_ACTION_NOACT;
    descriptor[d].BTCTRL.bit.BEATSIZE =
      DMA_BEAT_SIZE_BYTE;
    descriptor[d].BTCTRL.bit.DSTINC   = 0;
    descriptor[d].BTCTRL.bit.SRCINC   = 1;
    descriptor[d].BTCTRL.bit.STEPSEL  =
      DMA_STEPSEL_SRC;
    descriptor[d].BTCTRL.bit.STEPSIZE =
      DMA_ADDRESS_INCREMENT_STEP_SIZE_1;
    descriptor[d].BTCNT.reg   = descriptor_bytes;
    descriptor[d].DSTADDR.reg = (uint32_t)data_reg;
    // WARNING SRCADDRs MUST BE SET ELSEWHERE!
    if (d == numDescriptors-1) {
      descriptor[d].DESCADDR.reg = 0;
    } else {
      descriptor[d].DESCADDR.reg = (uint32_t)&descriptor[d+1];
    }
  }
  return true;
}

void Display_DMA::setFrameBuffer(uint16_t * fb) {
  screen = fb;
}

uint16_t * Display_DMA::getFrameBuffer(void) {
  return(screen);
}

void Display_DMA::setArea(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2) {
  arcada.startWrite();
  arcada.setAddrWindow(x1, y1, x2-x1+1, y2-y1+1);
}


void Display_DMA::refresh(void) {
  while (dma_busy);
  digitalWrite(ARCADA_TFT_DC, 0);
  ARCADA_TFT_SPI.transfer(ILI9341_SLPOUT);
  digitalWrite(ARCADA_TFT_DC, 1);
  digitalWrite(ARCADA_TFT_CS, 1);
  ARCADA_TFT_SPI.endTransaction();  

  fillScreen(ARCADA_CYAN);
  if (screen == NULL) {
    Serial.println("No screen framebuffer!");
    return;
  }
  Serial.println("DMA refresh");
  if (! setDmaStruct()) {
    arcada.haltBox("Failed to set up DMA");
  }
  // Initialize descriptor list SRC addrs
  for(int d=0; d<numDescriptors; d++) {
    descriptor[d].SRCADDR.reg = (uint32_t)screen + descriptor_bytes * (d+1);
    Serial.print("DMA descriptor #"); Serial.print(d); Serial.print(" $"); Serial.println(descriptor[d].SRCADDR.reg, HEX);
  }
  // Move new descriptor into place...
  memcpy(dptr, &descriptor[0], sizeof(DmacDescriptor));
  dma_busy = true;
  foo = this; // Save pointer to ourselves so callback (outside class) can reach members
  dma.loop(true);
  //  dma.setCallback(dma_callback);

  setAreaCentered();
  cancelled = false; 
  
  ARCADA_TFT_SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(ARCADA_TFT_CS, 0);
  digitalWrite(ARCADA_TFT_DC, 0);
  ARCADA_TFT_SPI.transfer(ILI9341_RAMWR);
  digitalWrite(ARCADA_TFT_DC, 1);

  Serial.print("DMA kick");
  dma.startJob();                // Trigger first SPI DMA transfer
}


void Display_DMA::stop(void) {
  Serial.println("DMA stop");

  cancelled = true;
  ARCADA_TFT_SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));  
  ARCADA_TFT_SPI.endTransaction();
  digitalWrite(ARCADA_TFT_CS, 1);
  digitalWrite(ARCADA_TFT_DC, 1);     
}

void Display_DMA::wait(void) {
  Serial.println("DMA wait");
}

uint16_t * Display_DMA::getLineBuffer(int j)
{ 
  return(&screen[j*ARCADA_TFT_WIDTH]);
}

/***********************************************************************************************
    DMA functions
 ***********************************************************************************************/

void Display_DMA::fillScreen(uint16_t color) {
  uint16_t *dst = &screen[0];
  for (int i=0; i<EMUDISPLAY_WIDTH*EMUDISPLAY_HEIGHT; i++)  {
    *dst++ = color;
  }
}


void Display_DMA::writeScreen(int width, int height, int stride, uint8_t *buf, uint16_t *palette16) {
  uint8_t *buffer=buf;
  uint8_t *src; 
  if (screen != NULL) {
  uint16_t *dst = &screen[0];
    int i,j;
    if (width*2 <= EMUDISPLAY_WIDTH) {
      for (j=0; j<height; j++)
      {
        src=buffer;
        for (i=0; i<width; i++)
        {
          uint16_t val = palette16[*src++];
          *dst++ = val;
          *dst++ = val;
        }
        dst += (EMUDISPLAY_WIDTH-width*2);
        if (height*2 <= EMUDISPLAY_HEIGHT) {
          src=buffer;
          for (i=0; i<width; i++)
          {
            uint16_t val = palette16[*src++];
            *dst++ = val;
            *dst++ = val;
          }
          dst += (EMUDISPLAY_WIDTH-width*2);      
        } 
        buffer += stride;      
      }
    }
    else if (width <= EMUDISPLAY_WIDTH) {
      dst += (EMUDISPLAY_WIDTH-width)/2;
      for (j=0; j<height; j++)
      {
        src=buffer;
        for (i=0; i<width; i++)
        {
          uint16_t val = palette16[*src++];
          *dst++ = val;
        }
        dst += (EMUDISPLAY_WIDTH-width);
        if (height*2 <= EMUDISPLAY_HEIGHT) {
          src=buffer;
          for (i=0; i<width; i++)
          {
            uint16_t val = palette16[*src++];
            *dst++ = val;
          }
          dst += (EMUDISPLAY_WIDTH-width);
        }      
        buffer += stride;  
      }
    }    
  }
}

#if EMU_SCALEDOWN == 2
  uint8_t last_even_line_red[NATIVE_WIDTH/2], last_even_line_green[NATIVE_WIDTH/2], last_even_line_blue[NATIVE_WIDTH/2];
#endif

void Display_DMA::writeLine(int width, int height, int stride, uint8_t *buf, uint16_t *palette16) {
  uint8_t *src=buf;
#if EMU_SCALEDOWN == 1
  uint16_t *dst = &screen[EMUDISPLAY_WIDTH*stride];
  for (int i=0; i<width; i++) {
    uint8_t val = *src++;
    *dst++=palette16[val];
  }
#elif EMU_SCALEDOWN == 2
  uint8_t *red_p = last_even_line_red;
  uint8_t *green_p = last_even_line_green;
  uint8_t *blue_p = last_even_line_blue;
  uint16_t color0, color1;
  if (stride % 2 == 0) { 
    for (int i=0; i<width/2; i++) {
      // extract and store the averaged colors (takes memory but saves time!)
      color0 = palette16[*src++];
      color1 = palette16[*src++];
      *blue_p++ = (color0 & 0x1F) + (color1 & 0x1F);
      color0 >>= 5; color1 >>= 5;
      *green_p++ = (color0 & 0x2F) + (color1  & 0x2F);
      color0 >>= 6; color1 >>= 6;
      *red_p++ = (color0 & 0x1F) + (color1 & 0x1F);
    }
  } else {
    uint16_t *dst = &screen[EMUDISPLAY_WIDTH*(stride-1)/2]; // stride skipped every other line
    for (int i=0; i<width/2; i++) {
      color0 = palette16[*src++];
      color1 = palette16[*src++];
      uint16_t color;
      uint8_t blue = *blue_p++ + (color0 & 0x1F) + (color1 & 0x1F);
      blue >>= 2;
      color0 >>= 5; color1 >>= 5;
      uint8_t green = *green_p++ + (color0 & 0x2F) + (color1 & 0x2F);
      green >>= 2;
      color0 >>= 6; color1 >>= 6;
      uint8_t red = *red_p++ + (color0 & 0x1F) + (color1 & 0x1F);;
      red >>= 2;
      
      color = red; color <<= 6; // rejoin into a color
      color |= green; color <<= 5;
      color |= blue;
      if (test_invert_screen) color = ~color;
      *dst++=__builtin_bswap16(color);
    }
  }
#else
  #error("Only scale 1 or 2 supported")
#endif
}

inline void Display_DMA::setAreaCentered(void) {
  setArea((ARCADA_TFT_WIDTH  - EMUDISPLAY_WIDTH ) / 2, 
  (ARCADA_TFT_HEIGHT - EMUDISPLAY_HEIGHT) / 2,
  (ARCADA_TFT_WIDTH  - EMUDISPLAY_WIDTH ) / 2 + EMUDISPLAY_WIDTH  - 1, 
  (ARCADA_TFT_HEIGHT - EMUDISPLAY_HEIGHT) / 2 + EMUDISPLAY_HEIGHT - 1);
}

#endif
