#ifndef IMAGE_PROVIDER_H_
#define IMAGE_PROVIDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

int InitializeCamera(tflite::ErrorReporter* error_reporter);
uint32_t GetImage(tflite::ErrorReporter* error_reporter, uint8_t* image_data);

#endif  // IMAGE_PROVIDER_H_
