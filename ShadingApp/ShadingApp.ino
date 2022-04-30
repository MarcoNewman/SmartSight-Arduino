#include "main_functions.h"

#include <TensorFlowLite.h>

#include "image_provider.h"

#include <SPI.h>

#include "LCD_low.h"
#include "LCD_draw.h"
#include <stdio.h>

// Globals, used for compatibility with Arduino-style sketches.
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  const int imgWidth = 128;
  const int imgHeight = 64;
  constexpr int size = imgWidth*imgHeight;
  uint8_t image[size];
  uint8_t blobs[size];

  int blob_extension = 5;

  int chunks = floor(size / 1000);
}

// The name of this function is important for Arduino compatibility.
void setup() {
  Serial.begin(500000);
  while (!Serial) ;
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // LCD CONFIG
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(PIN_TIME, OUTPUT);
  SET_CS;
  
  // Initialize SPI. By default the clock is 4MHz.
  SPI.begin();
  //Bump the clock to 8MHz. Appears to be the maximum.
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  //Fire up the SPI LCD
  Serial.println(F("Initialize_CFAG12864T3U3()"));
  Initialize_CFAG12864T2U2();
}

// The name of this function is important for Arduino compatibility.
void loop() {
  double loopTime = millis();
  // Get image from provider.
  int max_brightness = GetImage(error_reporter, imgWidth, imgHeight, 1, image);
  if (max_brightness != 0){
    // normalize and threshold
    int threshold = 120;
    int norm = max_brightness - threshold;
    for (int i=0; i<size; i++){
      if (image[i] > threshold){
        float normalized = (image[i] - threshold) / norm;
        if (normalized > 0.9){
          image[i] = 1;
        }
        else{
          image[i] = 0;
        }
      }
      else {
        image[i] = 0;
      }
    }

    for (int i=0; i<imgHeight; i++){
      for (int j=0; j<imgWidth; j++){
        bool neighbor = false;
        for (int x=0; x<blob_extension; x++){
          // Corners
          if (i-x > 0 && j-x >= 0){
            if (image[(j-x) + i*imgWidth]) neighbor = true;
          }
          if (i+x < imgHeight && j-x >= 0){
            if (image[(j+x) + (i-x)*imgWidth]) neighbor = true;
          }
          if (i-x >= 0 && j+x < imgWidth){ 
            if (image[(j-x) + (i+x)*imgWidth]) neighbor = true; 
          }
          if (i+x < imgHeight && i+x < imgWidth){
            if (image[(j+x) + (i+x)*imgWidth]) neighbor = true;
          }
          // Sides
          if (i-x >= 0){
            if (image[j + (i-x)*imgWidth]) neighbor = true;
          }
          if (i+x < imgHeight){
            if (image[j + (i+x)*imgWidth]) neighbor = true;
          }
          if (j-x >= 0){
            if (image[(j-x) + i*imgWidth]) neighbor = true;
          }
          if (j+x < imgWidth){
            if (image[(j+x) + i*imgWidth]) neighbor = true;
          }
          if (neighbor) break;
        }
        int buffer_x = j;
        int buffer_y = 7 - (floor(i/8));
        int bit_place = i % 8;
        if (buffer_y > 0 && buffer_y < 7 && buffer_x > 8 && buffer_x < 120){
          if (bit_place == 0 ){
            framebuffer[buffer_y][buffer_x] = 0;
          }
          if (image[j + i*imgWidth] || neighbor){
            framebuffer[buffer_y][buffer_x] = framebuffer[buffer_y][buffer_x] | 1 << 7 - bit_place;
            blobs[j + i*imgWidth] = 1;
          }
          else {
            blobs[j + i*imgWidth] = 0;
          }
        }
      }
    }
    Send_Framebuffer_To_Display();

    //Serial.println("CMD: STREAM IMAGE");
    //StreamImage();
  }
  Serial.print("NANO-> LOOP END IN: ");
  Serial.println(millis() - loopTime);
  //while (millis() < loopTime + 5000);
}

void StreamImage(){
  uint8_t* image_slice;
  for (int i=0; i<chunks; i++){
    int index = i * 1000;
    image_slice = &image[index];
    if (i == 0){
      Serial.println(size, DEC);
    }
    // Update SERIAL_TX_BUFFER_SIZE in /hardware/arduino/avr/cores/arduino/HardwareSerial.h
    Serial.write(image_slice, 1000);
  }
  int bytes_rem = size % 1000;
  Serial.write(image_slice, bytes_rem);
  Serial.println("NANO-> Image Sent");
}
