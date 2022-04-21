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

#include <TensorFlowLite.h>

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "models.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "tensorflow/lite/micro/testing/micro_test.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
  const int HANDSHAKE = 0;
  const int ON_REQUEST = 1;
  const int STREAM = 2;
  const int STATUS = 3;
  const int GET_128x128_IMAGE = 4;
  const int STREAM_128x128_IMAGE = 5;
  const int DETECT_FACE = 6;
  //const int GET_192x192_IMAGE = 4;
  //const int STREAM_192x192_FACE = 5;
  //const int COMPUTE_ACTIVATIONS = 6;
  //const int STREAM_ACTIVATIONS = 7;
  //const int TRAIN_CLASS_1 = 8;
  //const int TRAIN_CLASS_2 = 9;
  //const int CLASSIFY = 10;

  int mode = ON_REQUEST;
  // Default time between stream acquisition is 10 ms
  int streamDelay = 1;
  // String to store input of delay
  String delayStr;
  // Keep track of last data acquistion for delays
  unsigned long timeOfLastPxStream = 0;
  // Keep track of image pixel index
  int x = 0;
  int y = 0;

  tflite::ErrorReporter* error_reporter = nullptr;
  
  // MEDIAPIPE
  const tflite::Model* model_mediapipe = nullptr;
  tflite::MicroInterpreter* interpreter_mediapipe = nullptr;
  TfLiteTensor* image_mediapipe = nullptr;
  TfLiteTensor* boxes_mediapipe = nullptr;
  TfLiteTensor* scores_mediapipe = nullptr;
  constexpr int kImageSize_mediapipe = 128*128*3; //float32
  constexpr int kBoxesSize_mediapipe = 896*16;    //float32
  constexpr int kScoresSize_mediapipe = 896;      //float32
  constexpr int kTensorArenaSize_mediapipe = kImageSize_mediapipe+kBoxesSize_mediapipe+kScoresSize_mediapipe;
  constexpr int size = 150000;
  uint8_t tensor_arena_mediapipe[size];

}  // namespace

// The name of this function is important for Arduino compatibility.
void setup() {
  
  Serial.begin(115200);
  while (!Serial) ;
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  Serial.println("STARTING MODEL CONFIG");
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model_mediapipe = tflite::GetModel(g_mobilenet_model_data);
  if (model_mediapipe->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model_mediapipe->version(), TFLITE_SCHEMA_VERSION);
    return;
  }
  Serial.println("MODEL LOADED");

  // Pull in all TFLite operations
  static tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroInterpreter static_interpreter_mediapipe( 
    model_mediapipe,
    resolver,
    tensor_arena_mediapipe, size,
    error_reporter
  );
  interpreter_mediapipe = &static_interpreter_mediapipe;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter_mediapipe->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }
  TfLiteStatus reset_status = interpreter_mediapipe->ResetVariableTensors();
  if (reset_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "ResetVariableTensors() failed");
    return;
  }
  Serial.println("TENSORS ALLOCATED");

  // Get information about the memory area to use for the model's input.
  image_mediapipe = interpreter_mediapipe->input(0);
  boxes_mediapipe = interpreter_mediapipe->output(0);
  scores_mediapipe = interpreter_mediapipe->output(1);
  Serial.println("Finished Setup");
}

// The name of this function is important for Arduino compatibility.
void loop() {
  // If we're streaming
  if (mode == STREAM) {
    if (millis() - timeOfLastPxStream >= streamDelay && x < 128) {
      int index = (x * 128) + y;
      int8_t rgb[3];
      rgb[0] = image_mediapipe->data.int8[index];
      rgb[1] = image_mediapipe->data.int8[index+1];
      rgb[2] = image_mediapipe->data.int8[index+2];
      String outstr = String(String(rgb[0], DEC)+","+String(rgb[1], DEC)+","+String(rgb[2], DEC));
      Serial.println(outstr);
      if (++y == 128){
        y = 0;
        x++;
      }

      timeOfLastPxStream =  millis();
    }
  }

  if (Serial.available() > 0) {    // is a character available?
    int inByte = Serial.read();       // get the character
    switch (inByte){
      case HANDSHAKE:
        Serial.println("Message received.");
        break;
      case ON_REQUEST:
        mode = ON_REQUEST;
        x = 0;
        y = 0;
        break;
      case STREAM:
        x = 0;
        y = 0;
        mode = STREAM;
        break;
      case STATUS:
        // get some info
        Serial.println("Model Info:");
        Serial.println(model_mediapipe->version());
        Serial.println(interpreter_mediapipe->arena_used_bytes());
        Serial.println(interpreter_mediapipe->initialization_status());
        Serial.println(interpreter_mediapipe->inputs_size());
        Serial.println(interpreter_mediapipe->outputs_size());
        Serial.println(interpreter_mediapipe->tensors_size());
        Serial.print("Number of dimensions: ");
        Serial.println(boxes_mediapipe->dims->size,DEC);
        Serial.print("Dim 1 size: ");
        Serial.println(boxes_mediapipe->dims->data[0],DEC);
        Serial.print("Dim 2 size: ");
        Serial.println(boxes_mediapipe->dims->data[1],DEC);
        Serial.print("Dim 3 size: ");
        Serial.println(boxes_mediapipe->dims->data[2],DEC);
        Serial.print("Dim 4 size: ");
        Serial.println(boxes_mediapipe->dims->data[3],DEC);
        Serial.print("Type: ");
        Serial.println(boxes_mediapipe->type,DEC);
        

        break;

      case GET_128x128_IMAGE:
        // Get image from provider.
        if (kTfLiteOk != GetImage(error_reporter, kCols_mediapipe, kRows_mediapipe, kChannels_mediapipe,
                                  image_mediapipe->data.int8)) {
          TF_LITE_REPORT_ERROR(error_reporter, "Image capture failed.");
        }
        break;
      case DETECT_FACE:
        // Run the model on this input and make sure it succeeds.
        if (kTfLiteOk != interpreter_mediapipe->Invoke()) {
          TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
        }
        const float* raw_boxes = boxes_mediapipe->data.f;
        const float* raw_scores = scores_mediapipe->data.f;
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
