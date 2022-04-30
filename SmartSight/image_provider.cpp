#include "image_provider.h"

/*
 * The sample requires the following third-party libraries to be installed and
 * configured:
 *
 * Arducam
 * -------
 * 1. Download https://github.com/ArduCAM/Arduino and copy its `ArduCAM`
 *    subdirectory into `Arduino/libraries`. Commit #e216049 has been tested
 *    with this code.
 * 2. Edit `Arduino/libraries/ArduCAM/memorysaver.h` and ensure that
 *    "#define OV2640_MINI_2MP_PLUS" is not commented out. Ensure all other
 *    defines in the same section are commented out.
 *
 * JPEGDecoder
 * -----------
 * 1. Install "JPEGDecoder" 1.8.0 from the Arduino library manager.
 * 2. Edit "Arduino/Libraries/JPEGDecoder/src/User_Config.h" and comment out
 *    "#define LOAD_SD_LIBRARY" and "#define LOAD_SDFAT_LIBRARY".
 */

#if defined(ARDUINO) && !defined(ARDUINO_ARDUINO_NANO33BLE)
#define ARDUINO_EXCLUDE_CODE
#endif  // defined(ARDUINO) && !defined(ARDUINO_ARDUINO_NANO33BLE)

#ifndef ARDUINO_EXCLUDE_CODE

// Required by Arducam library
#include <SPI.h>
#include <Wire.h>
#include <memorysaver.h>
// Arducam library
#include <ArduCAM.h>

// Checks that the Arducam library has been correctly configured
#if !(defined OV2640_MINI_2MP_PLUS)
#error Please select the hardware platform and camera module in the Arduino/libraries/ArduCAM/memorysaver.h
#endif

// The size of our temporary buffer for holding
// JPEG data received from the Arducam module
#define MAX_JPEG_BYTES 40000
// The pin connected to the Arducam Chip Select
#define CS 2

// Camera library instance
ArduCAM myCAM(OV2640, CS);

// Length of the JPEG data currently in the buffer
uint32_t jpeg_length = 0;

bool cameraInitialized = false;

// Get the camera module ready
TfLiteStatus InitCamera(tflite::ErrorReporter* error_reporter) {
  TF_LITE_REPORT_ERROR(error_reporter, "Attempting to start Arducam");
  // Enable the Wire library
  Wire.begin();
  // Configure the CS pin
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  // initialize SPI
  SPI.begin();
  //Bump the clock to 8MHz. Appears to be the maximum.
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  // Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);
  // Test whether we can communicate with Arducam via SPI
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  uint8_t test;
  test = myCAM.read_reg(ARDUCHIP_TEST1);
  if (test != 0x55) {
    TF_LITE_REPORT_ERROR(error_reporter, "Can't communicate with Arducam");
    delay(1000);
    return kTfLiteError;
  }
  // Use JPEG capture mode, since it allows us to specify
  // a resolution smaller than the full sensor frame
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  //myCAM.OV2640_set_Light_Mode(Advanced_AWB);
  //myCAM.OV2640_set_Brightness(6);
  // Store Large Image in FIFO buffer parsing.
  myCAM.OV2640_set_JPEG_size(OV2640_640x480);
  delay(100);
  return kTfLiteOk;
}

// Begin the capture and wait for it to finish
TfLiteStatus PerformCapture(tflite::ErrorReporter* error_reporter) {
  TF_LITE_REPORT_ERROR(error_reporter, "Starting capture");
  // Make sure the buffer is emptied before each capture
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  // Start capture
  myCAM.start_capture();
  // Wait for indication that it is done
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
  }
  TF_LITE_REPORT_ERROR(error_reporter, "Image captured");
  delay(50);
  // Clear the capture done flag
  myCAM.clear_fifo_flag();
  return kTfLiteOk;
}

// Read data from the camera module into a local buffer
TfLiteStatus ReadData(tflite::ErrorReporter* error_reporter, uint8_t* jpeg_buffer) {
  // This represents the total length of the JPEG data
  jpeg_length = myCAM.read_fifo_length();
  TF_LITE_REPORT_ERROR(error_reporter, "Reading %d bytes from Arducam",
                       jpeg_length);
  // Ensure there's not too much data for our buffer
  if (jpeg_length > MAX_JPEG_BYTES) {
    TF_LITE_REPORT_ERROR(error_reporter, "Too many bytes in FIFO buffer (%d)",
                         MAX_JPEG_BYTES);
    return kTfLiteError;
  }
  if (jpeg_length == 0) {
    TF_LITE_REPORT_ERROR(error_reporter, "No data in Arducam FIFO buffer");
    return kTfLiteError;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  for (int index = 0; index < jpeg_length; index++) {
    jpeg_buffer[index] = SPI.transfer(0x00);
  }
  delayMicroseconds(15);
  TF_LITE_REPORT_ERROR(error_reporter, "Finished reading");
  myCAM.CS_HIGH();
  return kTfLiteOk;
}


int InitializeCamera(tflite::ErrorReporter* error_reporter){
  if (!cameraInitialized){
    TfLiteStatus init_status = InitCamera(error_reporter);
    if (init_status != kTfLiteOk) {
      TF_LITE_REPORT_ERROR(error_reporter, "InitCamera failed");
      return 1;
    }
  } else {
    return 1;
  }
  return 0;
}

// Get an image from the camera module
uint32_t GetImage(tflite::ErrorReporter* error_reporter, uint8_t* image_data) {
  cameraInitialized = InitializeCamera(error_reporter);
  TfLiteStatus capture_status = PerformCapture(error_reporter);
  if (capture_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "PerformCapture failed");
  }

  TfLiteStatus read_data_status = ReadData(error_reporter, image_data);
  if (read_data_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "ReadData failed");
    return 0;
  }

  return jpeg_length;
}

#endif  // ARDUINO_EXCLUDE_CODE
