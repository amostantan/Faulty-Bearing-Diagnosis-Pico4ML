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
#include <stdlib.h>

#include "main_functions.h"
#include <math.h>

#include "accelerometer_handler.h"
#include "magic_wand_model_data.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;
int input_length;

// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kTensorArenaSize = 120 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

extern "C" void* sbrk(int increment);

int begin_ind = 0;
float predict_array_1[7];
float predict_array_2[7];
float predict_array_3[7];
float predict_array_4[7];
bool last_predict_normal = true;
// The name of this function is important for Arduino compatibility.
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Setting up memory and model");
  delay(500);
  Serial.println("Setting up...");

  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  static tflite::MicroErrorReporter micro_error_reporter;  // NOLINT
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroMutableOpResolver<6> micro_op_resolver; 
  micro_op_resolver.AddQuantize();
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddMean();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddAveragePool2D();
  micro_op_resolver.AddDequantize();


  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  interpreter->AllocateTensors();

  // Allocate memory from the tensor_arena for the model's tensors.
  // TfLiteStatus allocate_status = interpreter->AllocateTensors();
  // if (allocate_status != kTfLiteOk) {
  // TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
  //   return;
  // }

  // Obtain pointer to the model's input tensor.
  model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) ||                // Expect 4 dims
    (model_input->dims->data[0] != 1) ||             // batch = 1
    (model_input->dims->data[1] != 250) ||           // window_size 
    (model_input->dims->data[2] != 3) ||            // width = 3 Dimensions
    (model_input->dims->data[3] != 1) ||
    (model_input->type != kTfLiteFloat32)) {          // input type int8 Or float32
  TF_LITE_REPORT_ERROR(error_reporter,
                       "Bad input tensor parameters in model");
  return;
  }

  input_length = model_input->bytes / sizeof(float);  

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "Set up failed\n");
  }

  TfLiteTensor* input = interpreter->input(0);

  Serial.print("Input type:");
  Serial.println(input->type);

  Serial.print("Total tensor size: ");
  Serial.println(input_length);

  Serial.print("Input tensor bytes: ");
  Serial.println(model_input->bytes);

  TfLiteTensor* input_tensor = interpreter->input(0);
  TF_LITE_REPORT_ERROR(error_reporter,
  "Input shape: %d x %d x %d x %d",
  input_tensor->dims->data[0],
  input_tensor->dims->data[1],
  input_tensor->dims->data[2],
  input_tensor->dims->data[3]);
  
  Serial.println("Set up Successful");
  
  pinMode(LED_BUILTIN, OUTPUT);

  printFreeRAM();  // Rough estimate of free RAM

  Serial.print("TFLM Arena Used: ");
  Serial.println(interpreter->arena_used_bytes());

  delay(10000);
}

void printFreeRAM() {
  char* heap_end = (char*) sbrk(0);
  char stack_var;
  char* stack_ptr = &stack_var;

  int free_ram = stack_ptr - heap_end;
  Serial.print("Estimated free RAM: ");
  Serial.print(free_ram);
  Serial.println(" bytes");
}

void loop() {
  // If you want to reset, press the run button on Pico4ML
  // Attempt to read new data from the accelerometer.
  bool got_data =
      ReadAccelerometer(error_reporter, model_input->data.f, input_length);
  // If there was no new data, wait until next time.
  if (!got_data) return;

  // Serial.println("Reading...");

  // Run inference, and report any error.
  TfLiteStatus invoke_status = interpreter->Invoke();
  // Serial.print("Invoke status: ");
  // Serial.println(invoke_status);  // Print numeric status

  if (invoke_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed on index: %d\n",
                         begin_index);
    while(1);
    return;
  }
  
  // printFreeRAM();
  TfLiteTensor* output_tensor = interpreter->output(0);
  const float* output_data = interpreter->output(0)->data.f;
  
  // float class0_score = output_scale * (output_data[0] - output_zero_point);
  float class0_score = output_data[0];
  Serial.println("");
  Serial.print("Normal");
  Serial.print(": ");
  Serial.print(class0_score);
  predict_array_1[begin_ind] = class0_score;

  // float class1_score = output_scale * (output_data[1] - output_zero_point);
  float class1_score = output_data[1];
  Serial.print(" Outer");
  Serial.print(": ");
  Serial.print(class1_score);
  predict_array_2[begin_ind] = class1_score;

  // float class2_score = output_scale * (output_data[2] - output_zero_point);
  float class2_score = output_data[2];
  Serial.print(" InnerRail");
  Serial.print(": ");
  Serial.print(class2_score);
  predict_array_3[begin_ind] = class2_score;


  // float class3_score = output_scale * (output_data[3] - output_zero_point);
  float class3_score = output_data[3];
  Serial.print(" Cage");
  Serial.print(": ");
  Serial.print(class3_score);
  predict_array_4[begin_ind] = class3_score;

  begin_ind++;

  if (begin_ind >= 6){
    int checker = 0;
    // Serial.println("");
    // Serial.println(checker);
    if (fabs(predict_array_1[5] - predict_array_1[1]) > 1){
      // Serial.println(fabs(predict_array_1[5] - predict_array_1[1]));
      checker++;
      // Serial.println("hit 1");
    }
    if (fabs(predict_array_2[5] - predict_array_2[1]) > 1){
      // Serial.println(fabs(predict_array_2[4] - predict_array_2[1]));
      checker++;
      // Serial.println("hit 2");
    }
    if (fabs((predict_array_3[5] - predict_array_3[1]) > 1)){
      // Serial.println(fabs(predict_array_3[4] - predict_array_3[1]));
      checker++;
      // Serial.println("hit 3");
    }
    if (fabs(predict_array_4[5] - predict_array_4[1]) > 0.8){
      // Serial.println(fabs(predict_array_4[4] - predict_array_4[1]));
      checker++;
    }
    if (checker > 0){
      // Serial.println("Calibrating");
      if (last_predict_normal == true){
      for (int led = 0; led < 5; led++){
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
      }
      else{
      }
      // // Serial.println(checker);
      
    }
    else{
      float check_biggest[4];
      int check_value = 0;
      float max;
      check_biggest[0] = class0_score; //Normal
      check_biggest[1] = class1_score; //Outer
      check_biggest[2] = class2_score; //InnerRail
      check_biggest[3] = class3_score; //Gun
      max = check_biggest[0];
      for (int i = 0; i < 4; i++){
        if (check_biggest[i] > max){
          max = check_biggest[i];
        }
      }
      for (int i = 0; i < 4; i++){
        if (max == check_biggest[i]){
          check_value = i;
        }
      }
      if (check_value == 0){
        // Serial.println("");
        // Serial.println("Normal");
        digitalWrite(LED_BUILTIN, LOW);
        last_predict_normal = true;
      }
      else if (check_value == 1){
        // Serial.println("");
        // Serial.println("Outer");
        digitalWrite(LED_BUILTIN, HIGH);
        last_predict_normal = false;

      }
      else if (check_value == 2){
        digitalWrite(LED_BUILTIN, HIGH);
          // Serial.println("");  
        // Serial.println("Inner Rail");
        last_predict_normal = false;
      }
      else if (check_value == 3){
        // Serial.println("");  
        // Serial.println("Gun");
        digitalWrite(LED_BUILTIN, HIGH);
        last_predict_normal = false;
      }
    }
    begin_ind = 0;
  }
}
