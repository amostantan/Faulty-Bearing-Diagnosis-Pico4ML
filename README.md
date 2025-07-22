# FaultyBearing-Diagnosis-Pico4ML
This github repository goes through the flow of how the faulty bearing diagnostic system was designed.

The content goes as follows:
1. Data collection
    1. Printing of Accelerometer values on Serial monitor
    2. Extracting of values from serial monitor & recording onto Excel sheets
2. Data Processing & Training of Model
    1. Data Preprocessing
    2. Model Architecture and Training
    3. Quantization of Model
3. Model conversion from format .tflite > .c
4. Implement the model into Pico4ML
5. Observe model outputs
