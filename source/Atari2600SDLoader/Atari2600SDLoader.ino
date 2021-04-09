/**
    Load Atari 2600 games from SD Card
    @file Atari2600SDLoader.ino
    @author Johannes Le Roux (@dadecoza)
    @version 1.1 03/04/2021
*/

#include <SPI.h>
#include <SD.h>
#include "kernels.h"

#define ATARI_POWER 8
#define SRAM_OUTPUT_EN 14
#define SRAM_WRITE_EN 15
#define SHIFT_DATA 16
#define SHIFT_OUTPUT_EN 17 
#define SHIFT_LATCH 18
#define SHIFT_CLK 19
#define SD_CHIP_SELECT 10

#define KERNEL_NO_SD 0
#define KERNEL_NO_FILE 1

#define SRAM_SIZE 8192

File myFile;

void setup() {
  pinMode(SHIFT_OUTPUT_EN, OUTPUT);
  pinMode(SRAM_OUTPUT_EN, OUTPUT);
  pinMode(SRAM_WRITE_EN, OUTPUT);
  initIo();

  //Wait until the Atari is switched off before we continue
  pinMode(ATARI_POWER, INPUT);
  while (digitalRead(ATARI_POWER)) delay(5);

  //Disable UART (probably not necessary)
  UCSR0B &= ~(_BV(TXEN0));  //disable UART TX
  UCSR0B &= ~(_BV(RXEN0));  //disable UART RX

  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_DATA, OUTPUT);

  prepareToWrite();
  //Wait for SD card to initialize
  if (!SD.begin(SD_CHIP_SELECT)) {
    writeSramFromFlash(KERNEL_NO_SD);
  } else {
    //Finally write the ROM file to SRAM
    writeSRAM();
  }
  initIo();
}


//Nothing to loop, once ROM is written to SRAM our job is done
void loop() {
  delay(5);
}

//Set all the pins in a safe state for the Atari
void initIo() {
  digitalWrite(SHIFT_OUTPUT_EN, HIGH); //Disable Output on shift registers
  digitalWrite(SRAM_WRITE_EN, HIGH); //Disable SRAM Write
  digitalWrite(SRAM_OUTPUT_EN, LOW); //Enable SRAM output
  //We user PORTD (pins 0-7) as our data bus
  PORTD = B00000000; //Set PORTD all pins low
  DDRD = B00000000; //Set PORTD direction register for input
}

//Enable output on data bus and shift registers, disable output on SRAM
void prepareToWrite() {
  digitalWrite(SHIFT_OUTPUT_EN, LOW); //Enable output on shift registers
  digitalWrite(SRAM_OUTPUT_EN, HIGH); //Disable SRAM output
  DDRD = B11111111; //Set PORTD direction register for output
}

//There are two "ROMs" with error messages stored in PROGMEM. One for when we can't read the SD card, and one for when we can't find file.
void writeSramFromFlash(int p) {
  byte b;
  int address=0;
  for (int i=0; i<(SRAM_SIZE/512); i++) {
    for (int k=0; k<512; k++) {
      b = pgm_read_byte_near((p ? kernelNoFile : kernelNoSD) + k);
      writeByte(address, b);
      address++;
    }
  }
}

//Find the next file on the SD card and write it to the SRAM
void writeSRAM() {
  int index = getNextIndex();
  if (!index) {
    writeSramFromFlash(kernelNoFile);
    return;
  }
  int address = 0;
  String fileName = String(index) + ".bin";
  myFile = SD.open(fileName);
  byte b;
  //We will write to all addresses repeating the file contents
  while (address < SRAM_SIZE) {
    if (myFile.available()) {
      b = myFile.read();
    } else {
      myFile.seek(0);
      b = myFile.read();
    }
    writeByte(address, b);
    address++;
  }
  myFile.close();
}

//Clock the shift registers for the requested address lines
void setAddr(int address) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);
  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

//Write a byte to a specified address
void writeByte(int address, byte data) {
  PORTD = data;
  setAddr(address);
  digitalWrite(SRAM_WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(SRAM_WRITE_EN, HIGH);
}

//Get the next index number of a ROM file and update the index.dat file
int getNextIndex() {
  int i = getFileIndex() + 1;
  for (int n = i; n < 50; n++) {
    String fileName = String(n) + ".bin";
    if (SD.exists(fileName)) {
      return updateFileIndex(n);
    }
  }
  for (int n = 0; n < i; n++) {
    String fileName = String(n) + ".bin";
    if (SD.exists(fileName)) {
      return updateFileIndex(n);
    }
  }
  return 0;
}

//Write a ROM file index number in the index.dat file, replacing the file if it already exists
int updateFileIndex(int i) {
  if (SD.exists("index.dat")) SD.remove("index.dat");
  myFile = SD.open("index.dat", FILE_WRITE);
  if (myFile) {
    myFile.write(i);
    myFile.close();
  }
  return i;
}

//Read the current ROM file index number from the index.dat file
int getFileIndex() {
  int i = 0;
  myFile = SD.open("index.dat");
  if ((myFile) && (myFile.available())) {
    i = myFile.read();
    myFile.close();
    return i;
  }
  return updateFileIndex(0);
}
