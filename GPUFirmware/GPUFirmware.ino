// Display resolution: 9x16 (Left half: 9x8, Right half: 9x8)
// 4-bit bit-bang pins: 
//     Left half: (+) 0-3; (-) 4-7; 
//     Right half: (+) 8-10,12; (-) 13,A0-A2;
// PWM brightness pin: 11

#include <Arduino.h>
#include <array>

// 9 rows (y: 0 to 8) and 8 columns (x: 0 to 7)
std::array<std::array<std::array<int, 8>, 2>, 9> charliePlexMap = {{
    // Row 0 (was Row 1 in 1-indexed mapping)
    {{
        {1, 2, 3, 4, 5, 6, 7, 8},    // + mapping (Pos)
        {0, 0, 0, 0, 0, 0, 0, 0}     // - mapping (Neg)
    }},
    // Row 1 
    {{
        {0, 2, 3, 4, 5, 6, 7, 8},
        {1, 1, 1, 1, 1, 1, 1, 1}
    }},
    // Row 2 
    {{
        {0, 1, 3, 4, 5, 6, 7, 8},
        {2, 2, 2, 2, 2, 2, 2, 2}
    }},
    // Row 3 
    {{
        {0, 1, 2, 4, 5, 6, 7, 8},
        {3, 3, 3, 3, 3, 3, 3, 3}
    }},
    // Row 4 
    {{
        {0, 1, 2, 3, 5, 6, 7, 8},
        {4, 4, 4, 4, 4, 4, 4, 4}
    }},
    // Row 5 
    {{
        {0, 1, 2, 3, 4, 6, 7, 8},
        {5, 5, 5, 5, 5, 5, 5, 5}
    }},
    // Row 6 
    {{
        {0, 1, 2, 3, 4, 5, 7, 8},
        {6, 6, 6, 6, 6, 6, 6, 6}
    }},
    // Row 7 
    {{
        {0, 1, 2, 3, 4, 5, 6, 8},
        {7, 7, 7, 7, 7, 7, 7, 7}
    }},
    // Row 8 
    {{
        {0, 1, 2, 3, 4, 5, 6, 7},
        {8, 8, 8, 8, 8, 8, 8, 8}
    }}
}};

struct Pixel {
    int x; // 0-indexed column (0 to 7)
    int y; // 0-indexed row (0 to 8)
    int a; // brightness of the pixel
};

struct CharliePlexPins {
    int pinPos; // Pin to drive HIGH
    int pinNeg; // Pin to drive LOW
};


CharliePlexPins getPinsForPixel(const Pixel& pixel)
{
    // Assumes pixel.x is in [0,7] and pixel.y is in [0,8]
    CharliePlexPins pins;
    pins.pinPos = charliePlexMap[pixel.y][0][pixel.x];
    pins.pinNeg = charliePlexMap[pixel.y][1][pixel.x];
    return pins;
}

// A Frame is represented as a nested array:
// First dimension: half (0 = left, 1 = right)
// Second dimension: rows (0 to 8)
// Third dimension: columns (0 to 7)
using Frame = std::array<std::array<std::array<Pixel, COLS>, ROWS>, HALVES>;


//----------------------------------------------------------
// Pin Definitions for Bit-Banging
//----------------------------------------------------------
// Left half: positive side pins are digital 0–3; negative side pins are digital 4–7.
const int leftPosPins[4] = {0, 1, 2, 3};
const int leftNegPins[4] = {4, 5, 6, 7};

// Right half: positive side pins: 8, 9, 10, 12; negative side pins: 13, A0, A1, A2.
const int rightPosPins[4] = {8, 9, 10, 12};
const int rightNegPins[4] = {13, A0, A1, A2};


//----------------------------------------------------------
// Output Functions for a Pixel
//----------------------------------------------------------
// This function takes an integer 'value' (0–15), and for each of the 4 bits,
// writes HIGH if that bit is 1 and LOW if it is 0 on the provided pins.
void setBitBangOutput(const int pins[4], int value) {
  for (int i = 0; i < 4; i++) {
    // (1 << i) checks bit i; bit0 corresponds to pins[0], etc.
    if (value & (1 << i)) {
      digitalWrite(pins[i], HIGH);
    } else {
      digitalWrite(pins[i], LOW);
    }
  }
}

// For the left panel:
void outputLeftPixel(const Pixel &pixel) {
  CharliePlexPins cp = getPinsForPixel(pixel);

  // Set the left panel pins as OUTPUT.
  for (int i = 0; i < 4; i++) {
    pinMode(leftPosPins[i], OUTPUT);
    pinMode(leftNegPins[i], OUTPUT);
  }
  
  // Convert the integer value to 4-bit output.
  setBitBangOutput(leftPosPins, cp.pos);
  setBitBangOutput(leftNegPins, cp.neg);
  
  // Set the brightness using the PWM brightness pin (pin 11).
  analogWrite(11, pixel.a);
  
  // Hold the state briefly (adjust delay as needed for multiplexing).
  delayMicroseconds(100);
  
  // Return pins to INPUT state.
  for (int i = 0; i < 4; i++) {
    pinMode(leftPosPins[i], INPUT);
    pinMode(leftNegPins[i], INPUT);
  }
}

// For the right panel:
void outputRightPixel(const Pixel &pixel) {
  CharliePlexPins cp = getPinsForPixel(pixel);

  for (int i = 0; i < 4; i++) {
    pinMode(rightPosPins[i], OUTPUT);
    pinMode(rightNegPins[i], OUTPUT);
  }
  
  setBitBangOutput(rightPosPins, cp.pos);
  setBitBangOutput(rightNegPins, cp.neg);
  
  analogWrite(11, pixel.a);
  delayMicroseconds(100);
  
  for (int i = 0; i < 4; i++) {
    pinMode(rightPosPins[i], INPUT);
    pinMode(rightNegPins[i], INPUT);
  }
}

//----------------------------------------------------------
// Frame Definition and Refresh
//----------------------------------------------------------
// A full display is 9x16, composed of two 9x8 halves:
// half 0 = left; half 1 = right.
constexpr int ROWS = 9;
constexpr int COLS = 8;
constexpr int HALVES = 2;
using Frame = std::array<std::array<std::array<Pixel, COLS>, ROWS>, HALVES>;

// Refreshes the display by scanning through the frame and outputting lit pixels.
void refreshFrame(const Frame &frame) {
  // Process left half (index 0)
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (frame[0][y][x].a > 0) {
        outputLeftPixel(frame[0][y][x]);
      }
    }
  }
  // Process right half (index 1)
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (frame[1][y][x].a > 0) {
        outputRightPixel(frame[1][y][x]);
      }
    }
  }
}

//----------------------------------------------------------
// Arduino Setup and Loop
//----------------------------------------------------------
void setup() {
  // Initialize the PWM brightness pin.
  pinMode(11, OUTPUT);
  
  // Initialize all bit-bang pins (for simplicity, initializing digital pins 0-13 and analog pins A0-A2).
  for (int pin = 0; pin < 14; pin++) {
    pinMode(pin, INPUT);
  }
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
}

void loop() {
  // Create a frame and initialize all pixels with brightness 255.
  Frame frame;
  for (int half = 0; half < HALVES; half++) {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        frame[half][y][x] = { x, y, 255 };
      }
    }
  }
  
  // Refresh the display by scanning through the frame.
  refreshFrame(frame);
}