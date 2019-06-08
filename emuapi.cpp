#include "display_dma.h"
#include <Adafruit_Arcada.h>
extern Adafruit_Arcada arcada;

extern "C" {
#include "emuapi.h"
#include "nesstate.h"
}

extern Display_DMA tft;
extern char rom_filename_path[512];
static File file;


#define LOADMENU_SELECTIONS 3
#define LOADMENU_LOADSAVED  0
#define LOADMENU_LOADCLEAN  1
#define LOADMENU_DELETEFILE 2
const char *loadmenu_strings[LOADMENU_SELECTIONS] = {"Load Saved Game", "Start New Game", "Delete Save File"};



void emu_Halt(const char * error_msg) {
  Serial.println(error_msg);
  tft.stop();
  arcada.fillScreen(ARCADA_BLACK);
  arcada.haltBox(error_msg);
}

void emu_printf(const char *str)
{
  Serial.println(str);
}

void emu_printf(int val)
{
  Serial.println(val);
}

void emu_printi(int val)
{
  Serial.println(val);
}

void * emu_Malloc(int size)
{
  void * retval =  malloc(size);
  if (!retval) {
    char error_msg[255];
    sprintf(error_msg, "emu_Malloc: failed to allocate %d bytes", size);
    emu_Halt(error_msg);
  }
  else {
    Serial.print("emu_Malloc: successfully allocated ");
    Serial.print(size); Serial.println(" bytes");
  }

  return retval;
}

void emu_Free(void * pt)
{
  emu_printf("Freeing memory");
  free(pt);
}

int emu_FileOpen(char * filename)
{
  int retval = 0;
  Serial.print("FileOpen: "); Serial.print(filename);

  if (file = arcada.open(filename, O_READ)) {
    Serial.println("...Success!");
    retval = 1;
  } else {
    char error_msg[255];
    sprintf(error_msg, "emu_FileOpen: failed to open file %s");
    emu_Halt(error_msg);
  }
  return (retval);
}

uint8_t *emu_LoadROM(char *filename) {
  Serial.print("LoadROM: "); Serial.print(filename);
  tft.stop();
  arcada.fillScreen(ARCADA_CYAN);
  arcada.infoBox("Loading ROM into FLASH memory...", 0);
  uint8_t *romdata = arcada.writeFileToFlash(filename, DEFAULT_FLASH_ADDRESS);
  Serial.printf(" into address $%08x", (uint32_t)romdata);
  if ((uint32_t)romdata == 0) {
    emu_Halt("Unable to load file into FLASH, maybe too large?");
  }
  arcada.fillScreen(ARCADA_BLACK);
  tft.refresh();
  return romdata;
}

int emu_FileRead(uint8_t* buf, int size)
{
  int retval = file.read(buf, size);
  if (retval != size) {
    emu_Halt("FileRead failed");
  }
  return (retval);
}

unsigned char emu_FileGetc(void) {
  unsigned char c;
  int retval = file.read(&c, 1);
  if (retval != 1) {
    emu_Halt("emu_FileGetc failed");
  }
  return c;
}


void emu_FileClose(void)
{
  file.close();
}

int emu_FileSize(char * filename)
{
  int filesize = 0;
  emu_printf("FileSize...");
  emu_printf(filename);

  if (file = arcada.open(filename, O_READ))
  {
    emu_printf("filesize is...");
    filesize = file.fileSize();
    emu_printf(filesize);
    file.close();
  }

  return (filesize);
}

int emu_FileSeek(int pos)
{
  file.seek(pos);
  return pos;
}

int emu_LoadFile(char * filename, char * buf, int numbytes) {
  int filesize = 0;

  emu_printf("LoadFile...");
  emu_printf(filename);

  if (file = arcada.open(filename, O_READ)) {
    filesize = file.fileSize();
    emu_printf(filesize);
    if (numbytes >= filesize)
    {
      if (file.read(buf, filesize) != filesize) {
        emu_Halt("File read failed");
      }
    }
    file.close();
  }

  return (filesize);
}

int emu_LoadFileSeek(char * filename, char * buf, int numbytes, int pos) {
  int filesize = 0;

  emu_printf("LoadFileSeek...");
  emu_printf(filename);

  if (file = arcada.open(filename, O_READ))
  {
    file.seek(pos);
    emu_printf(numbytes);
    if (file.read(buf, numbytes) != numbytes) {
      emu_printf("File read failed");
    }
    file.close();
  }

  return (filesize);
}



void emu_SaveState() {
#if defined(USE_SAVEFILES)
  char filename[512];
  strcpy(filename, rom_filename_path);
  char *fp = filename + strlen(filename);
  fp[0] = '.';
  fp[1] = 's';
  fp[2] = 'a';
  fp[3] = 'v';
  fp[4] = 0;
  if (arcada.exists(filename)) {
    arcada.remove(filename);
  }
  Serial.print("Saving state to file:");
  Serial.println(filename);
  state_save(filename);
#endif
}

void emu_LoadState() {
#if defined(USE_SAVEFILES)
  char filename[512];
  strncpy(filename, rom_filename_path, 500);
  char *fp = filename + strlen(filename);
  fp[0] = '.';
  fp[1] = 's';
  fp[2] = 'a';
  fp[3] = 'v';
  fp[4] = 0;
  if (arcada.exists(filename)) {
    Serial.println("Found savefile");
    tft.stop();
    delay(50);
    arcada.fillScreen(ARCADA_BLUE);
    uint8_t selected = arcada.menu(loadmenu_strings, LOADMENU_SELECTIONS, ARCADA_WHITE, ARCADA_BLACK);
    Serial.printf("Selected %d\n", selected);
    if (selected ==  LOADMENU_LOADSAVED) {
      Serial.print("Loading state from file:");
      Serial.println(filename);
      state_load(filename);      
    }
    if (selected == LOADMENU_DELETEFILE) {
      arcada.remove(filename);
    }
    // either way, restart the display
    arcada.fillScreen(ARCADA_BLACK);
    tft.refresh();
  }
#endif
}

static uint16_t bLastState;
uint16_t button_CurState;
uint16_t emu_DebounceLocalKeys(void) {
  button_CurState = arcada.readButtons();

  uint16_t bClick = button_CurState & ~bLastState;
  bLastState = button_CurState;

  return bClick;
}

uint32_t emu_ReadKeys(void)  {
  return arcada.readButtons();
}
