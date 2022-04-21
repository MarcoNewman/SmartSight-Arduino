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

#include "image_provider.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
  const int HANDSHAKE = 0;
  const int ON_REQUEST = 1;
  const int STREAM_IMAGE = 2;
  const int STATUS = 3;
  const int CAPTURE_IMAGE = 4;
  const int STREAM_LOGITS = 5;
  //const int GET_192x192_IMAGE = 4;
  //const int STREAM_IMAGE_192x192_FACE = 5;
  //const int COMPUTE_ACTIVATIONS = 6;
  //const int STREAM_IMAGE_ACTIVATIONS = 7;
  //const int TRAIN_CLASS_1 = 8;
  //const int TRAIN_CLASS_2 = 9;
  //const int CLASSIFY = 10;

  int mode = ON_REQUEST;

  // Error Reporting
  tflite::ErrorReporter* error_reporter = nullptr;
  
  // RGB IMAGE BUFFERING
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

  uint8_t logits[1001];

  // TODO: CONTROL BUTTONS/REVERSE SERIAL CONTROL/ADD KNN CLASSIFIER
  // trainButton
  // classifyButton
  // scrollupClassifier
  // scrolldownClassifier

}  // namespace

// The name of this function is important for Arduino compatibility.
void setup() {
  
  Serial.begin(115200);
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
    int i = 0;
    while (i < 1001){
      if (Serial.available() > 0) {
        logits[i] = Serial.read();
        i++;
      }
    }
    Serial.print("logit 999: ");
    Serial.println(logits[999]);
    mode = ON_REQUEST;
  }

  // STATE MANAGEMENT
  if (Serial.available() > 0) {    // is a character available?
    int inByte = Serial.read();       // get the character
    switch (inByte){
      case HANDSHAKE:
        Serial.println("Message received.");
        break;
      case ON_REQUEST:
        mode = ON_REQUEST;
        x = 0;
        break;
      case STREAM_IMAGE:
        x = 0;
        mode = STREAM_IMAGE;
        break;
      case STREAM_LOGITS: // DO THIS NEXT!!
        x = 0;
        mode = STREAM_LOGITS;
        break;
      case STATUS:
        // get some info
        Serial.println(jpeg_length, DEC);
        break;
      case CAPTURE_IMAGE:
        // Get image from provider.
        jpeg_length = GetImage(error_reporter, img_size, img_size, 3, jpeg_buffer);
        chunks = floor(jpeg_length/1000);
        break;
      //case TRAIN_CLASS_1:
        // Process the inference results.
        //int8_t person_score = output->data.uint8[kPersonIndex];
        //int8_t no_person_score = output->data.uint8[kNotAPersonIndex];
        //RespondToDetection(error_reporter, person_score, no_person_score); //KNN STUFF

        //break;
    }
  }
}
