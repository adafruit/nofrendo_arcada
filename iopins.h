#ifndef IOPINS_H
#define IOPINS_H

//#include <Arduino.h>

#if defined(ARDUINO_GRAND_CENTRAL_M4) // TFT Shield
  #define TFT_SPI         SPI
  #define TFT_SERCOM      SERCOM7
  #define TFT_DC          9
  #define TFT_CS          10
  #define TFT_RST         -1  // unused
#endif


#endif
