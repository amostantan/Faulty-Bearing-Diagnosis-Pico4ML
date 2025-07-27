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

#include "accelerometer_handler.h"
#include "ICM20948.h"
#include <hardware/gpio.h>
#include "arduinoFFT.h"

#define REG_ADD_ACCEL_SMPLRT_DIV_1 0x10
#define REG_ADD_ACCEL_SMPLRT_DIV_2 0x11
#define REG_ADD_ACCEL_CONFIG 0x14

#define REG_VAL_BIT_ACCEL_DLPCFG_0 0x00  // bits [2:0] = 0 for max bandwidth filter (no/lowest filtering)
#define REG_VAL_BIT_ACCEL_FS_16g   0x18  // example value for ±16g FS bits [4:3] = 11 (binary)
#define REG_VAL_BIT_ACCEL_DLPF     0x20  // bit 5, enable accel DLPF

// Buffer, save the last 125 groups of 3 channel values
float save_data_int[1500];

// the latest position in the save_data buffer
int begin_index = 0;
// If there is not enough data to make inferences, then True
auto pending_initial_data = true;

IMU_EN_SENSOR_TYPE enMotionSensorType;

TfLiteStatus SetupAccelerometer(tflite::ErrorReporter *error_reporter) {
  i2c_init(I2C_PORT, 100 * 1000);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
  sleep_ms(1000);
  ICM20948::imuInit(&enMotionSensorType);

  // Wire.beginTransmission(ICM20948_ADDRESS);
  // Wire.write(REG_BANK_SEL);
  // Wire.write(REG_BANK_0);
  // Wire.endTransmission();

  //Switching to Bank 2
  ICM20948::I2C_WriteOneByte(REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_2);

  // Set accel sample rate divider registers to 0
  ICM20948::I2C_WriteOneByte(REG_ADD_ACCEL_SMPLRT_DIV_1, 0x00);
  ICM20948::I2C_WriteOneByte(REG_ADD_ACCEL_SMPLRT_DIV_2, 0x00);

  // Configure accelerometer: 
  // REG_VAL_BIT_ACCEL_DLPCFG_0 = accel digital low-pass filter setting
  // REG_VAL_BIT_ACCEL_FS_16g = accel full scale range ±16g
  // REG_VAL_BIT_ACCEL_DLPF = enable accel DLPF
  uint8_t accel_config = (REG_VAL_BIT_ACCEL_DLPCFG_0 | REG_VAL_BIT_ACCEL_FS_2g | REG_VAL_BIT_ACCEL_DLPF);
  ICM20948::I2C_WriteOneByte(REG_ADD_ACCEL_CONFIG, accel_config);

  // Read back the registers to verify settings
  uint8_t smplrt1 = ICM20948::I2C_ReadOneByte(REG_ADD_ACCEL_SMPLRT_DIV_1);
  uint8_t smplrt2 = ICM20948::I2C_ReadOneByte(REG_ADD_ACCEL_SMPLRT_DIV_2);
  uint8_t config = ICM20948::I2C_ReadOneByte(REG_ADD_ACCEL_CONFIG);

  // Select bank 0
  ICM20948::I2C_WriteOneByte(REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0);

  char debug_buffer[64];
  snprintf(debug_buffer, sizeof(debug_buffer), "SMPLRT_DIV_1: %02X, SMPLRT_DIV_2: %02X, CONFIG: %02X", smplrt1, smplrt2, config);
  TF_LITE_REPORT_ERROR(error_reporter, debug_buffer);

  if (IMU_EN_SENSOR_TYPE_ICM20948 != enMotionSensorType) {
    TF_LITE_REPORT_ERROR(error_reporter, "Failed to initialize IMU");
    return kTfLiteError;
  }

  TF_LITE_REPORT_ERROR(error_reporter, "Magic starts!");
  return kTfLiteOk;
}

static bool UpdateData() {

  float x = 0.0000f, y = 0.0000f, z = 0.0000f;
  if (!ICM20948::icm20948AccelRead(&x, &y, &z)) {
    return false;

  }
  // Serial.println("");
  // Serial.println(x, 4);
  // Serial.println(y, 4);
  // Serial.println(z,4 );


  // const float mean_x = -0.00848444f; //D4
  // const float mean_y = -0.0306859f;
  // const float mean_z =  1.98077424f;

  // const float std_x = 0.21680513f;
  // const float std_y = 0.14915132f;
  // const float std_z = 0.12954019f;

  const float mean_x = -0.01046191f; //Dataset 5
  const float mean_y = -0.03479379f;
  const float mean_z =  1.977515f;

  const float std_x = 0.20653662f;
  const float std_y = 0.13225297f;
  const float std_z = 0.14808142f;

  const float scaled_x = (x - mean_x) / std_x;
  const float scaled_y = (y - mean_y) / std_y;
  const float scaled_z = (z - mean_z) / std_z;


  save_data_int[begin_index++] = scaled_x;
  save_data_int[begin_index++] = scaled_y;
  save_data_int[begin_index++] = scaled_z;
  
  
  // Serial.println("");
  // Serial.println(begin_index);

//   printf("norm_x : %.2f , norm_y %.2f , norm_z %.2f \n", norm_x * 1000, norm_y *
//        1000, norm_z * 1000);
//   printf("%f\t%f\t%f\n", norm_x*1000, norm_y*1000, norm_z*1000);
//         time_us_32() - last_sample_millis);

  if (begin_index >= 1500) {
    begin_index = 0;
  }

  //  new_data = true;

  return true;
}

bool ReadAccelerometer(tflite::ErrorReporter *error_reporter, float *input,
                       int length) {
  bool new_data = false;

  while (ICM20948::dataReady()) {
    new_data = UpdateData();
  }
  if (!new_data) {
    return false;
  }
  // Check if we are ready to make predictions or are still waiting for more initial
  // data
  if (pending_initial_data && begin_index >= 375) {
    pending_initial_data = false;
  }

  // If we don't have enough data, return false
  if (pending_initial_data) {
    return false;
  }

  for (int i = 0; i < length; ++i) {
    int ring_array_index = begin_index + i - length;
    if (ring_array_index < 0) {
      ring_array_index += 750;
    }
    input[i] = save_data_int[ring_array_index]; 
  }
  return true;
}
