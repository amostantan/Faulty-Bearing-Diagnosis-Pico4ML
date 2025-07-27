# FaultyBearing-Diagnosis-Pico4ML
This github repository goes through the flow of how the faulty bearing diagnostic system was designed.

The applications used are: Arduino IDE, Jupyter Lab & VS Code

The content goes as follows:
1. Pico4ML Libraries & Board Manager in Arduino IDE
2. Data collection
    1. Printing of Accelerometer values on Serial monitor (Arduino IDE)
    2. Extracting of values from serial monitor & recording onto Excel sheets
3. Data Processing & Training of Model
    1. Data Preprocessing
    2. Model Architecture and Training
    3. Quantization of Model
4. Model conversion from format .tflite > .c
5. Implement the model into Pico4ML
6. Observe model outputs

# Pico4ML Libraries & Board Managers to add
![Alt text](Arduino_Images/Arduino_Lib.jpg)

Add the above library to the Arduino IDE

![Alt text](Arduino_Images/Arduino_Board.jpg)

Add the above board extension to Arduino IDE

![Alt text](Arduino_Images/Arduino_Pref.jpg)

