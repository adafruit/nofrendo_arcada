/*
  Copyright Frank Bösing, 2017
  GPL license at https://github.com/FrankBoesing/Teensy64
*/

#if defined(__SAMD51__)
#include <Adafruit_Arcada.h>
extern Adafruit_Arcada arcada;
extern volatile bool test_invert_screen;
#include "emuapi.h"
#include "display_dma.h"
//#if defined(USE_SPI_DMA)
//  #error("Must not have SPI DMA enabled in Adafruit_SPITFT.h")
//#endif

#include <Adafruit_ZeroDMA.h>
#include "wiring_private.h"  // pinPeripheral() function
#include <malloc.h>          // memalign() function

// Actually 50 MHz due to timer shenanigans below, but SPI lib still thinks it's 24 MHz
#define SPICLOCK 24000000

Adafruit_ZeroDMA dma;                  ///< DMA instance
DmacDescriptor  *dptr         = NULL;  ///< 1st descriptor
DmacDescriptor  *descriptor    = NULL; ///< Allocated descriptor list
int numDescriptors;
int descriptor_bytes;

static uint16_t *screen = NULL;

volatile bool paused = false;
volatile uint8_t ntransfer = 0;

Display_DMA *foo; // Pointer into class so callback can access stuff

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
  dmac_id  = ARCADA_TFT_SPI.getDMAC_ID_TX();
  data_reg = ARCADA_TFT_SPI.getDataRegister();
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

  Serial.println("DMA create");
  if (! paused) {
    if (! setDmaStruct()) {
      arcada.haltBox("Failed to set up DMA");
    }
  }
  // Initialize descriptor list SRC addrs
  for(int d=0; d<numDescriptors; d++) {
    descriptor[d].SRCADDR.reg = (uint32_t)screen + descriptor_bytes * (d+1);
    Serial.print("DMA descriptor #"); Serial.print(d); Serial.print(" $"); Serial.println(descriptor[d].SRCADDR.reg, HEX);
  }
  // Move new descriptor into place...
  memcpy(dptr, &descriptor[0], sizeof(DmacDescriptor));

  setAreaCentered();
  
  ARCADA_TFT_SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(ARCADA_TFT_CS, 0);
  digitalWrite(ARCADA_TFT_DC, 0);
  ARCADA_TFT_SPI.transfer(ILI9341_RAMWR);
  digitalWrite(ARCADA_TFT_DC, 1);

  Serial.print("DMA kick");

  dma.loop(true);
  dma.startJob();                // Trigger first SPI DMA transfer
  paused = false; 
}


void Display_DMA::stop(void) {
  Serial.println("DMA stop");

  paused = true;
  dma.abort();
  delay(100);
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


#if 0
// NOT CURRENTLY BEING USED IN NOFRENDO.
// If it does get used, may need to handle 2x2 palette methodology.
// See also DrawScreen() in nofrendo_arcada.ino.
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
#endif

#if EMU_SCALEDOWN == 1

void Display_DMA::writeLine(int width, int height, int stride, uint8_t *buf, uint16_t *palette16) {
  uint16_t *dst = &screen[EMUDISPLAY_WIDTH*stride];
  while(width--) *dst++ = palette16[*buf++]; // palette16 already byte-swapped
}

#elif EMU_SCALEDOWN == 2

static uint8_t last_line[NATIVE_WIDTH];

void Display_DMA::writeLine(int width, int height, int stride, uint8_t *buf, uint32_t *paletteRB, uint16_t *paletteG) {
  if(!(stride & 1)) {
    // Even line number: just copy into last_line buffer for later
    memcpy(last_line, buf, width);
    // (Better still, if we could have nes_ppu.c render directly into
    // alternating scanline buffers, we could avoid the memcpy(), but
    // the EMU_SCALEDOWN definition doesn't reach there and it might
    // require some ugly changes (or the right place to do this might
    // be in nofrendo_arcada.ino). For now though, the memcpy() really
    // isn't a huge burden in the bigger scheme.)
  } else {
    // Odd line number: average 2x2 pixels using last_line and buf.
    uint8_t  *src1 = last_line, // Prior scanline
             *src2 = buf;       // Current scanline
    uint16_t *dst  = &screen[EMUDISPLAY_WIDTH*(stride-1)/2];
    uint32_t  rb;
    uint16_t  g, rgb;
    uint8_t   idx0, idx1, idx2, idx3;
    width /= 2;
    while(width--) {
      idx0 = *src1++; // Palette index of upper-left pixel
      idx1 = *src2++; // Palette index of lower-left pixel
      idx2 = *src1++; // Palette index of upper-right pixel
      idx3 = *src2++; // Palette index of lower-right pixel
      // Accumulate four RB and G values...
      rb   = paletteRB[idx0] + paletteRB[idx1] +
             paletteRB[idx2] + paletteRB[idx3]; // Accumulate R+B
      g    = paletteG[idx0]  + paletteG[idx1]  +
             paletteG[idx2]  + paletteG[idx3];  // Accumulate G
      // RB pallette data is in the form 00RRRRRRRR000BBBBBBBB00000000000,
      // so accumulating 4 pixels yields RRRRRRRRRR0BBBBBBBBBB00000000000.
      // G palette data is in the form   0000000GGGGGGGG0,
      // so accumulating 4 pixels yields 00000GGGGGGGGGG0.
      // Hold my beer...
      rgb  = ((rb >> 16) & 0b1111100000011111) |
             ( g         & 0b0000011111100000);
      // Preso!              RRRRRGGGGGGBBBBB
      //if (test_invert_screen) rgb = ~rgb;
      *dst++  = __builtin_bswap16(rgb); // Store big-endian
    }
  }
}

#else
  #error("Only scale 1 or 2 supported")
#endif

inline void Display_DMA::setAreaCentered(void) {
  setArea((ARCADA_TFT_WIDTH  - EMUDISPLAY_WIDTH ) / 2, 
  (ARCADA_TFT_HEIGHT - EMUDISPLAY_HEIGHT) / 2,
  (ARCADA_TFT_WIDTH  - EMUDISPLAY_WIDTH ) / 2 + EMUDISPLAY_WIDTH  - 1, 
  (ARCADA_TFT_HEIGHT - EMUDISPLAY_HEIGHT) / 2 + EMUDISPLAY_HEIGHT - 1);
}

#endif
