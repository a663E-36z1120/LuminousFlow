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
//  delay(1);
}

// Outputs a pixel on the right half.
void outputRightPixel(const Pixel &pixel) {
  CharliePlexPins cp = getPinsForPixel(pixel);
  analogWrite(11, pixel.a);
  setBitBangOutput(rightPosPins, cp.pos);
  setBitBangOutput(rightNegPins, cp.neg);
//  delay(1);
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
// Comms Protocol Parsing
//----------------------------------------------------------

/**
 * Parses incoming serial lines of the form:
 *   H,half,y,x,brightness
 * Example:
 *   H,0,3,5,128
 * 
 * Where:
 *   - half is 0 or 1
 *   - y is in [0..8]
 *   - x is in [0..7]
 *   - brightness is in [0..255]
 */
void parseSerial() {
  // If there's no data available, just return
  if (Serial.available() == 0) {
    return;
  }

  // Read a line up to newline
  String line = Serial.readStringUntil('\n');
  line.trim(); // remove any trailing \r or spaces

  // Quick check: does it start with "H,"?
  if (!line.startsWith("H,")) {
    // Not matching our protocol, ignore
    return;
  }

  // Remove "H," prefix
  line.remove(0, 2); // remove first 2 characters "H,"

  // Now we expect: half,y,x,brightness
  // We'll parse using indexOf and substring.
  int comma1 = line.indexOf(','); // half
  if (comma1 == -1) return;       // malformed
  int half = line.substring(0, comma1).toInt();
  line.remove(0, comma1 + 1);

  int comma2 = line.indexOf(','); // y
  if (comma2 == -1) return;
  int y = line.substring(0, comma2).toInt();
  line.remove(0, comma2 + 1);

  int comma3 = line.indexOf(','); // x
  if (comma3 == -1) return;
  int x = line.substring(0, comma3).toInt();
  line.remove(0, comma3 + 1);

  // The remainder is brightness
  int brightness = line.toInt();

  // Validate ranges
  if (half < 0 || half >= HALVES) return;
  if (y < 0 || y >= ROWS) return;
  if (x < 0 || x >= COLS) return;
  if (brightness < 0) brightness = 0;
  if (brightness > 255) brightness = 255;

  // Update the frame
  frame[half][y][x].a = brightness;
}


//----------------------------------------------------------
// Arduino Setup and Loop
//----------------------------------------------------------
void setup() {
  // Initialize the PWM brightness pin.
  pinMode(11, OUTPUT);
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2A |= (1 << WGM20);
  TCCR2B |= (0 << CS22) | (0 << CS21) | (1 << CS20);


  // Enable PWM output on Pin 11 (OC2A)
  TCCR2A |= (1 << COM2A1);
  
  // Initialize all bit-bang pins (digital pins 0-13 and analog pins A0-A2).
  for (int pin = 0; pin < 14; pin++) {
    pinMode(pin, OUTPUT);
  }
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  

// Initialize a full frame by setting each Pixel's coordinates and a brightness of 255.
  for (int half = 0; half < HALVES; half++) {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        frame[half][y][x].x = x;
        frame[half][y][x].y = y;
        frame[half][y][x].a = 255;  // brightness
      }
    }
  }

  Serial.begin(115200); // maxxing out the baud rate because why not lol
}


void loop() {
    parseSerial();
    refreshFrame(frame);
}
