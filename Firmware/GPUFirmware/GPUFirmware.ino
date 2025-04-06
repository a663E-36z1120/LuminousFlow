#include <Arduino.h>

//----------------------------------------------------------
// Constants
//----------------------------------------------------------
#define ROWS 9       // Number of rows
#define COLS 8       // Number of columns per half
#define HALVES 2     // Left half + Right half

// For receiving the new binary protocol: 1 header + 144 brightness bytes = 145
#define LED_ROWS 9   // 9 total rows
#define LED_COLS 16  // 16 total columns (8 per half)
#define FRAME_SIZE  (1 + LED_ROWS * LED_COLS)  // 145
#define FRAME_HEADER 0xFF

//----------------------------------------------------------
// Charlie-Plex Mapping Array
//----------------------------------------------------------
// Each element is an integer (0–15), interpreted as a 4-bit value.
// For example, if the value is 8 (binary 1000), then when bit-banged onto 4 pins,
// only the highest-order bit (bit 3) will be HIGH while bits 0–2 are LOW.
const int charliePlexMap[ROWS][2][COLS] = {
  { // Row 0
    {1, 2, 3, 4, 5, 6, 7, 8},    
    {0, 0, 0, 0, 0, 0, 0, 0}     
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
  int x;  // 0-indexed column (0..7 within its half)
  int y;  // 0-indexed row (0..8)
  int a;  // brightness (0–255)
};

struct CharliePlexPins {
  int pos; // mapping value (0–15) for positive side
  int neg; // mapping value (0–15) for negative side
};

//----------------------------------------------------------
// Helper Functions
//----------------------------------------------------------

// Returns the mapping values (4-bit number) for the given pixel.
CharliePlexPins getPinsForPixel(const Pixel &pixel) {
  CharliePlexPins cp;
  cp.pos = charliePlexMap[pixel.y][0][pixel.x];
  cp.neg = charliePlexMap[pixel.y][1][pixel.x];
  return cp;
}

// Bit-bangs a 4-bit integer value onto an array of 4 pins.
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
// Left half: positive side pins: D0–D3; negative side pins: D4–D7
const int leftPosPins[4] = {0, 1, 2, 3};
const int leftNegPins[4] = {4, 5, 6, 7};

// Right half: positive side pins: D8, D9, D10, D12; negative side pins: D13, A0, A1, A2
const int rightPosPins[4] = {8, 9, 10, 12};
const int rightNegPins[4] = {13, A0, A1, A2};

//----------------------------------------------------------
// Pixel Output Functions
//----------------------------------------------------------

// Outputs a pixel on the left half
void outputLeftPixel(const Pixel &pixel) {
  analogWrite(11, pixel.a);
  CharliePlexPins cp = getPinsForPixel(pixel);
  setBitBangOutput(leftPosPins, cp.pos);
  setBitBangOutput(leftNegPins, cp.neg);
}

// Outputs a pixel on the right half
void outputRightPixel(const Pixel &pixel) {
  analogWrite(11, pixel.a);
  CharliePlexPins cp = getPinsForPixel(pixel);
  setBitBangOutput(rightPosPins, cp.pos);
  setBitBangOutput(rightNegPins, cp.neg);
}

//----------------------------------------------------------
// Global Frame Array
//----------------------------------------------------------
// The full display is 9×16, split into two halves (left and right),
// each with 9 rows and 8 columns.
Pixel frame[HALVES][ROWS][COLS];

// Refreshes the display by scanning through each half & pixel
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
// Parsing the New Binary Protocol
//----------------------------------------------------------

// We'll buffer 145 bytes: 1 header (0xFF) + 144 brightness bytes
static uint8_t serialBuffer[FRAME_SIZE];
static int bufferIndex = 0;

// Reads new frames from Serial (if available)
void parseSerial() {
  while (Serial.available()) {
    uint8_t incoming = Serial.read();

    // If this is the first byte in a potential frame, we expect 0xFF
    if (bufferIndex == 0 && incoming != FRAME_HEADER) {
      // Not the correct header, ignore and keep waiting
      continue;
    }

    // Store the incoming byte
    serialBuffer[bufferIndex++] = incoming;

    // If we've collected all 145 bytes, we have a complete frame
    if (bufferIndex == FRAME_SIZE) {
      // Decode the brightness data into our global 'frame'
      int offset = 1; // skip the 0xFF header

      for (int row = 0; row < LED_ROWS; row++) {
        for (int col = 0; col < LED_COLS; col++) {
          uint8_t brightness = serialBuffer[offset++];
          
          // Figure out which half (0 or 1), and the column within that half
          int half = (col < COLS) ? 0 : 1;
          int colInHalf = (half == 0) ? col : (col - COLS);

          // Update the Pixel object
          frame[half][row][colInHalf].a = brightness;
        }
      }

      // Done reading this frame
      bufferIndex = 0;
    }
  }
}

//----------------------------------------------------------
// Arduino Setup
//----------------------------------------------------------
void setup() {
  // Initialize the PWM brightness pin (timer2, pin 11)
  pinMode(11, OUTPUT);
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2A |= (1 << WGM20); // Phase-correct PWM
  TCCR2B |= (0 << CS22) | (0 << CS21) | (1 << CS20); // No prescaler
  TCCR2A |= (1 << COM2A1); // Enable PWM on OC2A
  
  // Initialize bit-bang pins (D0..D13, A0..A2)
  for (int pin = 0; pin < 14; pin++) {
    pinMode(pin, OUTPUT);
  }
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);

  // Initialize global frame array
  for (int half = 0; half < HALVES; half++) {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        frame[half][y][x].x = x;
        frame[half][y][x].y = y;
        frame[half][y][x].a = 0;  // default brightness
      }
    }
  }

  // Start Serial (115200 or whichever you set on the PC side)
  Serial.begin(115200);
}

//----------------------------------------------------------
// Main Loop
//----------------------------------------------------------
void loop() {
  // 1) Parse incoming data (if any) into 'frame'
  parseSerial();

  // 2) Refresh the display using the current 'frame' brightness
  refreshFrame(frame);
}
