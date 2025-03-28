#include <Arduino.h>

//----------------------------------------------------------
// Constants
//----------------------------------------------------------
#define ROWS 9
#define COLS 8
#define HALVES 2

//----------------------------------------------------------
// Charlie-Plex Mapping Array
//----------------------------------------------------------
// Each element is an integer (0–15) that will be interpreted as a 4-bit value.
// For example, if the value is 8 (binary 1000), then when bit-banged onto 4 pins,
// only the highest-order bit (bit 3) will be HIGH while bits 0–2 are LOW.
const int charliePlexMap[ROWS][2][COLS] = {
  { // Row 0
    {1, 2, 3, 4, 5, 6, 7, 8},    // Positive mapping (for each column)
    {0, 0, 0, 0, 0, 0, 0, 0}     // Negative mapping
  },
  { // Row 1
    {0, 2, 3, 4, 5, 6, 7, 8},
    {1, 1, 1, 1, 1, 1, 1, 1}
  },
  { // Row 2
    {0, 1, 3, 4, 5, 6, 7, 8},
    {2, 2, 2, 2, 2, 2, 2, 2}
  },
  { // Row 3
    {0, 1, 2, 4, 5, 6, 7, 8},
    {3, 3, 3, 3, 3, 3, 3, 3}
  },
  { // Row 4
    {0, 1, 2, 3, 5, 6, 7, 8},
    {4, 4, 4, 4, 4, 4, 4, 4}
  },
  { // Row 5
    {0, 1, 2, 3, 4, 6, 7, 8},
    {5, 5, 5, 5, 5, 5, 5, 5}
  },
  { // Row 6
    {0, 1, 2, 3, 4, 5, 7, 8},
    {6, 6, 6, 6, 6, 6, 6, 6}
  },
  { // Row 7
    {0, 1, 2, 3, 4, 5, 6, 8},
    {7, 7, 7, 7, 7, 7, 7, 7}
  },
  { // Row 8
    {0, 1, 2, 3, 4, 5, 6, 7},
    {8, 8, 8, 8, 8, 8, 8, 8}
  }
};

//----------------------------------------------------------
// Data Structures
//----------------------------------------------------------
struct Pixel {
  int x; // 0-indexed column (0 to 7 within a half)
  int y; // 0-indexed row (0 to 8)
  int a; // brightness (0–255)
};

struct CharliePlexPins {
  int pos; // mapping value for positive side (0–15)
  int neg; // mapping value for negative side (0–15)
};

//----------------------------------------------------------
// Helper Functions
//----------------------------------------------------------

// Returns the mapping values (as a 4-bit number) for the given pixel.
CharliePlexPins getPinsForPixel(const Pixel &pixel) {
  CharliePlexPins cp;
  cp.pos = charliePlexMap[pixel.y][0][pixel.x];
  cp.neg = charliePlexMap[pixel.y][1][pixel.x];
  return cp;
}

// Bit-bangs a 4-bit integer value onto an array of 4 pins.
// For each bit in 'value', sets the corresponding pin HIGH if that bit is 1, LOW if 0.
void setBitBangOutput(const int pins[4], int value) {
  for (int i = 0; i < 4; i++) {
    if (value & (1 << i)) {
      digitalWrite(pins[i], HIGH);
    } else {
      digitalWrite(pins[i], LOW);
    }
  }
}

//----------------------------------------------------------
// Pin Definitions for Bit-Banging
//----------------------------------------------------------
// Left half: positive side pins: digital 0–3; negative side pins: digital 4–7.
const int leftPosPins[4] = {0, 1, 2, 3};
const int leftNegPins[4] = {4, 5, 6, 7};

// Right half: positive side pins: digital 8, 9, 10, 12; negative side pins: digital 13, A0, A1, A2.
const int rightPosPins[4] = {8, 9, 10, 12};
const int rightNegPins[4] = {13, A0, A1, A2};

//----------------------------------------------------------
// Pixel Output Functions
//----------------------------------------------------------

// Outputs a pixel on the left half.
void outputLeftPixel(const Pixel &pixel) {
  analogWrite(11, pixel.a);
  CharliePlexPins cp = getPinsForPixel(pixel);
  setBitBangOutput(leftPosPins, cp.pos);
  setBitBangOutput(leftNegPins, cp.neg);
  delay(1);
}

// Outputs a pixel on the right half.
void outputRightPixel(const Pixel &pixel) {
  CharliePlexPins cp = getPinsForPixel(pixel);
  analogWrite(11, pixel.a);
  setBitBangOutput(rightPosPins, cp.pos);
  setBitBangOutput(rightNegPins, cp.neg);
  delay(1);
}

//----------------------------------------------------------
// Global Frame Array
//----------------------------------------------------------
// The full display is 9x16, split into two halves (left and right),
// each with 9 rows and 8 columns.
Pixel frame[HALVES][ROWS][COLS];

// Refreshes the display by scanning through the frame and outputting lit pixels.
void refreshFrame(Pixel frameArray[][ROWS][COLS]) {
  // Process left half (index 0)
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (frameArray[0][y][x].a > 0) {
        outputLeftPixel(frameArray[0][y][x]);
      }
    }
  }
  // Process right half (index 1)
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (frameArray[1][y][x].a > 0) {
        outputRightPixel(frameArray[1][y][x]);
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
//  digitalWrite(11, HIGH);
  
  // Initialize all bit-bang pins (digital pins 0-13 and analog pins A0-A2).
  for (int pin = 0; pin < 14; pin++) {
    pinMode(pin, OUTPUT);
  }
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  
  // Initialize the frame: set each Pixel's coordinates and a brightness of 255.
  for (int half = 0; half < HALVES; half++) {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        frame[half][y][x].x = x;
        frame[half][y][x].y = y;
        frame[half][y][x].a = 255;  // Full brightness
      }
    }
  }
//  int y = 0; int x = 0;
//  frame[1][y][x].x = 0;
//  frame[1][y][x].y = 0;
//  frame[1][y][x].a = 255;
}



void loop() {
  // Refresh the display continuously.
//  refreshFrame(frame);
analogWrite(11, 64);
// LEFT POS
digitalWrite(0, HIGH);
digitalWrite(1, LOW);
digitalWrite(2, LOW);
digitalWrite(3, LOW);

// LEFT NEG
digitalWrite(4, LOW);
digitalWrite(5, LOW);
digitalWrite(6, LOW);
digitalWrite(7, LOW);

// RIGHT POS
digitalWrite(8, LOW);
digitalWrite(9, LOW);
digitalWrite(10, LOW);
digitalWrite(12, HIGH);

// RIGHT NEG
digitalWrite(13, LOW);
digitalWrite(A0, LOW);
digitalWrite(A1, LOW);
digitalWrite(A2, HIGH);

}
