/*
  Copyright Frank Bösing, 2017
  GPL license at https://github.com/FrankBoesing/Teensy64
*/

#ifndef _DISPLAY_DMA_
#define _DISPLAY_DMA_


#include <Adafruit_Arcada.h>
#include <Adafruit_ZeroDMA.h>

#ifdef __cplusplus
#include <Arduino.h>
#include <SPI.h>
#endif

#define DMA_FULL 1

#if ARCADA_TFT_WIDTH == 320
  // we assume this is an ILI9341, and ST7735's dont like overclocked SPI
  #define HIGH_SPEED_SPI
#endif

#define RGBVAL32(r,g,b)  ( (r<<16) | (g<<8) | b )
#define RGBVAL16(r,g,b)  ( (((r>>3)&0x1f)<<11) | (((g>>2)&0x3f)<<5) | (((b>>3)&0x1f)<<0) )
#define RGBVAL8(r,g,b)   ( (((r>>5)&0x07)<<5) | (((g>>5)&0x07)<<2) | (((b>>6)&0x3)<<0) )
#define R16(rgb) ((rgb>>8)&0xf8) 
#define G16(rgb) ((rgb>>3)&0xfc) 
#define B16(rgb) ((rgb<<3)&0xf8) 


#define NATIVE_WIDTH      256
#define NATIVE_HEIGHT     240
#define EMU_SCALEDOWN       2
#define EMUDISPLAY_WIDTH   (NATIVE_WIDTH / EMU_SCALEDOWN)
#define EMUDISPLAY_HEIGHT  (NATIVE_HEIGHT / EMU_SCALEDOWN)

#ifdef __cplusplus

#define SCREEN_DMA_MAX_SIZE 0xD000
#define SCREEN_DMA_NUM_SETTINGS (((uint32_t)((2 * ARCADA_TFT_HEIGHT * ARCADA_TFT_WIDTH) / SCREEN_DMA_MAX_SIZE))+1)


class Display_DMA
{
  public:
  	Display_DMA(void) {};

	  void setFrameBuffer(uint16_t * fb);
	  static uint16_t * getFrameBuffer(void);

	  void refresh(void);
	  void stop();
	  void wait(void);	
    uint16_t * getLineBuffer(int j);
            
    void setArea(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2); 
    void setAreaCentered(void);

    void writeScreen(int width, int height, int stride, uint8_t *buffer, uint16_t *palette16);
    void writeLine(int width, int height, int stride, uint8_t *buffer, uint16_t *palette16);
	  void fillScreen(uint16_t color);
	  void writeScreen(const uint16_t *pcolors);
	  void drawPixel(int16_t x, int16_t y, uint16_t color);
	  uint16_t getPixel(int16_t x, int16_t y);

    void dmaFrame(void); // End DMA-issued frame and start a new one
};

#endif
#endif
