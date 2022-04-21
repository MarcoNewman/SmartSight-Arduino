// This is a standard TensorFlow Lite model file that has been converted into a
// C data array, so it can be easily compiled into a binary for devices that
// don't have a file system. It was created using the command:
// xxd -i person_detect.tflite > person_detect_model_data.cc

#ifndef MODEL_DATA_H_
#define MODEL_DATA_H_

extern const unsigned char g_mediapipe_model_data[];
extern const int g_mediapipe_model_data_len;

extern const unsigned char g_mobilenet_model_data[];
extern const int g_mobilenet_model_data_len;

#endif
