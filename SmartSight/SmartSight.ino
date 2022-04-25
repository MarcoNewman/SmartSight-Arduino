#include "main_functions.h"

#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"

#include <Arduino_KNN.h>

#include "image_provider.h"

#define noOfButtons 3     //Exactly what it says; must be the same as the number of elements in buttonPins
#define bounceDelay 20    //Minimum delay before regarding a button as being pressed and debounced
#define minButtonPress 3  //Number of times the button has to be detected as pressed before the press is considered to be valid


// Globals, used for compatibility with Arduino-style sketches.
namespace {
  // Error Reporting
  tflite::ErrorReporter* error_reporter = nullptr;
  
  // Image streaming
  int camera_status = 0;
  uint8_t jpeg_buffer[20000];
  uint8_t chunks = 0;
  uint32_t jpeg_length;
  bool STREAM_IMAGE = false;

  // Logits streaming
  float logits[1001];
  
  // KNN
  const int CLASSES = 6;
  int CLASS_TRAINING = 0;
  KNNClassifier KNN(1001);

  // Control & IO
  const int LED_G = 2;
  const int LED_R = 3;     // set ledPin to on-board LED
  double LEDTimeout;

  const int PB_CLASSIFY = 4;
  const int PB_TRAIN = 5;
  const int PB_SELECT = 6;

  const int buttonPins[] = {4, 5, 6};      // Input pins to use, connect buttons between these pins and 0V
  uint32_t previousMillis[noOfButtons];       // Timers to time out bounce duration for each button
  uint8_t pressCount[noOfButtons];            // Counts the number of times the button is detected as pressed, when this count reaches minButtonPress button is regared as debounced 

  // Status
  const int FACE_DETECTED = 0;
  const int FACE_NOT_DETECTED = 1;

}  // namespace
  
// The name of this function is important for Arduino compatibility.
void setup() {
  
  Serial.begin(500000);
  while (!Serial) ;

  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  for (int i = 0; i < noOfButtons; ++i) {
    pinMode(buttonPins[i], INPUT_PULLDOWN);
  }

  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  camera_status = InitializeCamera(error_reporter);

}

// The name of this function is important for Arduino compatibility.
void loop() {
  debounce();
  if (millis() > LEDTimeout + 1000){
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
  }
  delay(10);
}

void debounce() {
  uint8_t i;
  uint32_t currentMillis = millis();
  for (i = 0; i < noOfButtons; ++i) {
    if (!digitalRead(buttonPins[i])) {             //Input is high, button not pressed or in the middle of bouncing and happens to be high
        previousMillis[i] = currentMillis;        //Set previousMillis to millis to reset timeout
        pressCount[i] = 0;                        //Set the number of times the button has been detected as pressed to 0
      } else {
      if (currentMillis - previousMillis[i] > bounceDelay) {
        previousMillis[i] = currentMillis;        //Set previousMillis to millis to reset timeout
        ++pressCount[i];
        if (pressCount[i] == minButtonPress) {    //Button has been debounced. Call function to do whatever you want done.
          if (i < 2){
            RunCV(i);
          } else {
            CLASS_TRAINING++;
            if (CLASS_TRAINING == CLASSES)
              CLASS_TRAINING = 0;
          }                 
        }
      }
    }
  }
}

void RunCV(int action){
  // CAPTURE IMAGE
  if (camera_status == 1) Serial.println("NANO-> Camera Not Initialized");
  else {
    jpeg_length = GetImage(error_reporter, jpeg_buffer);
    if (jpeg_length){
      chunks = floor(jpeg_length/1000);
      STREAM_IMAGE = true;
    }   
  }
  // STREAM IMAGE
  if (STREAM_IMAGE) {
    Serial.println("CMD: STREAM IMAGE");
    StreamImage();
    STREAM_IMAGE = false;
  }

  // GET LOGITS
  while (Serial.available() == 0) continue;
  int inByte = Serial.read();
  if (inByte == FACE_DETECTED){
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_R, LOW);
    Serial.println("CMD: STREAM LOGITS");
    StreamLogits();
    if (action == 0){//Classify
      int classification = KNN.classify(logits,3);
      Serial.print("NANO-> Classification: ");
      char* personStr;
      switch (classification){
        case 0: personStr = "Marco"; break;
        case 1: personStr = "Zach"; break;
        case 2: personStr = "Caleb"; break;
        case 3: personStr = "Veronika"; break;
        case 4: personStr = "Nisha"; break;
        case 5: personStr = "NEW PERSON"; break;
      }
      Serial.println(personStr);
      Serial.print("NANO-> Confidence: ");
      Serial.println(KNN.confidence());
    }
    else if (action == 1){//Train
      KNN.addExample(logits, CLASS_TRAINING);
      Serial.print("NANO-> Training for: ");
      char* personStr;
      switch (CLASS_TRAINING){
        case 0: personStr = "Marco"; break;
        case 1: personStr = "Zach"; break;
        case 2: personStr = "Caleb"; break;
        case 3: personStr = "Veronika"; break;
        case 4: personStr = "Nisha"; break;
        case 5: personStr = "NEW PERSON"; break;
      }
      Serial.println(personStr);
    }
    LEDTimeout = millis();
  }
  else if (inByte == FACE_NOT_DETECTED){
    Serial.println("NANO-> No face detected");
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    LEDTimeout = millis();
  }
  Serial.println("NANO-> CV COMPLETE");
}

void StreamImage(){
  uint8_t* jpeg_slice;
  for (int i=0; i<chunks; i++){
    int index = i * 1000;
    jpeg_slice = &jpeg_buffer[index];
    if (i == 0){
      Serial.println(jpeg_length, DEC);
    }
    // Update SERIAL_TX_BUFFER_SIZE in /hardware/arduino/avr/cores/arduino/HardwareSerial.h
    Serial.write(jpeg_slice, 1000);
  }
  int bytes_rem = jpeg_length % 1000;
  Serial.write(jpeg_slice, bytes_rem);
  Serial.println("NANO-> Image Sent");
}

void StreamLogits(){
  int i = 0;
  while (i < 1001){
    if (Serial.available() > 0) {
      logits[i] = Serial.read();
      i++;
    }
  }
  Serial.println("NANO-> Logits Received");
}