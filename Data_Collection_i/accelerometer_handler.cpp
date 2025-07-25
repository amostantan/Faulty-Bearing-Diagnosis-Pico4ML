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
#include <math.h>
#include "arduinoFFT.h"
#include <Arduino.h>
#include <Wire.h>

#define REG_VAL_BIT_ACCEL_DLPCFG_0 0x00  // bits [2:0] = 0 for max bandwidth filter (no/lowest filtering)
#define REG_VAL_BIT_ACCEL_FS_16g   0x18  // example value for ±16g FS bits [4:3] = 11 (binary)
#define REG_VAL_BIT_ACCEL_DLPF     0x20  // bit 5, enable accel DLPF

#define REG_ADD_ACCEL_SMPLRT_DIV_1 0x10
#define REG_ADD_ACCEL_SMPLRT_DIV_2 0x11
#define REG_ADD_ACCEL_CONFIG 0x14

IMU_EN_SENSOR_TYPE enMotionSensorType;

TfLiteStatus SetupAccelerometer(tflite::ErrorReporter *error_reporter) {
  i2c_init(I2C_PORT, 100 * 1000);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
  sleep_ms(1000);
  ICM20948::imuInit(&enMotionSensorType);

  //Switching to Bank 2
  ICM20948::I2C_WriteOneByte(REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_2);

  // Set accel sample rate divider registers to 0
  ICM20948::I2C_WriteOneByte(REG_ADD_ACCEL_SMPLRT_DIV_1, 0x00);
  ICM20948::I2C_WriteOneByte(REG_ADD_ACCEL_SMPLRT_DIV_2, 0x00);

  // Configure accelerometer: 
  // REG_VAL_BIT_ACCEL_DLPCFG_2 = accel digital low-pass filter setting
  // REG_VAL_BIT_ACCEL_FS_16g = accel full scale range ±16g
  // REG_VAL_BIT_ACCEL_DLPF = enable accel DLPF
  uint8_t accel_config = (REG_VAL_BIT_ACCEL_DLPCFG_0 | REG_VAL_BIT_ACCEL_FS_16g | REG_VAL_BIT_ACCEL_DLPF);
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

static bool UpdateData(tflite::ErrorReporter *error_reporter ) {
  // for (int cycle = 0; cycle < 129; cycle++){
  // unsigned long startTime = millis();
  float x = 0.0000f, y = 0.0000f, z = 0.0000f;
  char buf[64];
  if (!ICM20948::icm20948AccelRead(&x, &y, &z)) {
    return false;
  }
  snprintf(buf, sizeof(buf), "x: %.4f y: %.4f z: %.4f", x, y, z);
  TF_LITE_REPORT_ERROR(error_reporter, "%s", buf);

  return true;
}

bool ReadAccelerometer(tflite::ErrorReporter *error_reporter) {
  bool new_data = false;

  while (ICM20948::dataReady()) {
    new_data = UpdateData(error_reporter);
  }
  if (!new_data) {
    return false;
  }
  return true;
}

