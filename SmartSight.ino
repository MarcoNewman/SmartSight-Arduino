/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "main_functions.h"

#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"

#include <Arduino_KNN.h>

#include "image_provider.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
  const int HANDSHAKE = 0;
  // MODES
  const int ON_REQUEST = 1;
  const int STREAM_IMAGE = 2;
  const int STREAM_LOGITS = 3;

  // CONTROLS
  const int INITIALIZE = 4;
  const int CAPTURE_IMAGE = 5;
  const int FACE_DETECTED = 6;
  const int FACE_NOT_DETECTED = 7;

  const int TRAIN_1 = 8;
  const int TRAIN_2 = 9;
  const int CLASSIFY = 10;

  int mode = ON_REQUEST;

  // Error Reporting
  tflite::ErrorReporter* error_reporter = nullptr;
  
  // RGB IMAGE BUFFERING
  int camera_status = 0;
  constexpr int img_size = 240;
  // constexpr int kImageSize_mediapipe = img_size*img_size*3;
  // uint8_t image[kImageSize_mediapipe];
  // uint8_t* image_row;//[kImageSize_mediapipe / 128];
  uint8_t jpeg_buffer[20000];
  uint8_t chunks = 0;
  uint8_t* jpeg_slice;
  uint32_t jpeg_length;
  // Keep track of datastream chunk
  int x = 0;

  int i = 0;
  int logits_i = 0;
  int buffer_i;
  float logits[250];
  uint8_t buffer[4];
  
  

  // TODO: CONTROL BUTTONS/REVERSE SERIAL CONTROL/ADD KNN CLASSIFIER
  // trainButton
  // classifyButton
  // scrollupClassifier
  // scrolldownClassifier
  const int INPUTS = 250;
  const int CLASSES = 3; 
  const int EXAMPLES_PER_CLASS = 5;

  KNNClassifier KNN(250);

}  // namespace

// The name of this function is important for Arduino compatibility.
void setup() {
  
  Serial.begin(250000);
  while (!Serial) ;
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  Serial.println("Finished Setup");
}

// The name of this function is important for Arduino compatibility.
void loop() {
  // If we're streaming
  if (mode == STREAM_IMAGE) {
    int index = x * 1000;
    jpeg_slice = &jpeg_buffer[index];
    if (x == 0){
      Serial.println(jpeg_length, DEC);
    }
    if (x < chunks) {
      // Update SERIAL_TX_BUFFER_SIZE in /hardware/arduino/avr/cores/arduino/HardwareSerial.h
      Serial.write(jpeg_slice, 1000);
      x++;
    } else if (x == chunks){
      int jpeg_bytes = jpeg_length % 1000;
      Serial.write(jpeg_slice, jpeg_bytes);
      mode = ON_REQUEST;
    }
  }
  if (mode == STREAM_LOGITS) {
    if (Serial.available() > 0) {
      buffer_i = i % 4;
      buffer[buffer_i] = Serial.read();
      if (buffer_i == 3 && logits_i < 250){
        logits[logits_i] = (buffer[0]<<24)|(buffer[1]<<16)|(buffer[2]<<8)|(buffer[3]);
        logits_i++;
      }
      i++;
    }
    if (i == 1000){
      Serial.println("Logits Received");
      mode = ON_REQUEST;
    }
  }

  // STATE MANAGEMENT
  if (Serial.available() > 0 && mode != STREAM_LOGITS && mode != STREAM_LOGITS) {    // is a character available?
    int inByte = Serial.read();       // get the character
    switch (inByte){
      case HANDSHAKE:
        mode = ON_REQUEST;
        Serial.println("Message received.");
        break;
      
      // MODES
      case ON_REQUEST:
        mode = ON_REQUEST;
        x = 0;
        break;
      
      // ACTIONS/RESPONSES
      case CAPTURE_IMAGE:
        // Get image from provider.
        if (camera_status == 0){
          jpeg_length = GetImage(error_reporter, jpeg_buffer);
          chunks = floor(jpeg_length/1000);
          x = 0;
          mode = STREAM_IMAGE;
        } else {
          Serial.println("Camera Not Initialized");
        }
        break;
      case FACE_DETECTED:
        // TOGGLE LEDS
        i = 0;
        logits_i = 0;
        mode = STREAM_LOGITS;
        break;
      case FACE_NOT_DETECTED:
        // TOGGLE LEDS
        Serial.println("No Face Detected");
        break;
      
      // CONTROL
      case INITIALIZE:
        camera_status = InitializeCamera(error_reporter);
        if (camera_status == 0){
          Serial.println("Camera Initialized");
        } else {
          Serial.println("Camera Not Initialized");
        }
        break;
      case TRAIN_1:
        KNN.addExample(logits, 0);
        Serial.print("Example added for class: ");
        Serial.println(0);
        break;
      case TRAIN_2:
        KNN.addExample(logits, 1);
        Serial.print("Example added for class: ");
        Serial.println(1);
        break;
      case CLASSIFY:
        int classification = KNN.classify(logits,2);
        Serial.print("Classification Success:");
        Serial.println(classification);
        break;
    }
  }
}
