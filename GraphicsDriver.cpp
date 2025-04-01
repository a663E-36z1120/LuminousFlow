#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

// -----------------------------------------------------------------------------
// Global/Top-Level Variables
// -----------------------------------------------------------------------------

// Hard-code your LED matrix size
static const int LED_ROWS = 9;
static const int LED_COLS = 16;

// Number of brightness bins
static const int VAR_INTENSITY = 2; 

// Split the display into two halves of 8 columns each
static const int HALF_COLS = 8;

// Simulation domain parameters (matching the SPH engine's coordinate system)
static const double SIM_W = 0.8;   // half-width
static const double SIM_H = 0.9;   // total height

// Grid cell size for the hash grid approach
static const double CELL_SIZE = 0.1;


// -----------------------------------------------------------------------------
// Include Physics Engine
// -----------------------------------------------------------------------------
#include SPHEngine.cpp


// -----------------------------------------------------------------------------
// Helper to open and configure the serial port on Windows.
// Returns a valid HANDLE or INVALID_HANDLE_VALUE on error.
// -----------------------------------------------------------------------------
HANDLE openSerialPort(const char* portName, DWORD baudRate = CBR_115200) {
    HANDLE hSerial = CreateFileA(
        portName,
        GENERIC_READ | GENERIC_WRITE,
        0,               // No sharing
        NULL,            // No security
        OPEN_EXISTING,
        0,               // Non-overlapped I/O
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening port " << portName << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    // Configure serial parameters
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to get current serial parameters." << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = baudRate;  // 115200 by default
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to set serial parameters." << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Optionally set timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 1000;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "Failed to set timeouts." << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    std::cout << "Successfully opened " << portName << " at baud " << baudRate << std::endl;
    return hSerial;
}


// -----------------------------------------------------------------------------
// Hashgrid with variable brightness
//   - Partitions the SPH domain into a 9×16 grid (cellSize=0.1, simW=0.8, simH=0.9).
//   - Accumulates how many particles land in each cell.
//   - Converts the count to brightness in [0..255] using VAR_INTENSITY bins.
//
// The output is stored in ledFrame[LED_ROWS][LED_COLS].
// -----------------------------------------------------------------------------
void hashGrid(const std::vector<double>& positions,
              unsigned char ledFrame[LED_ROWS][LED_COLS])
{
    // 1) Clear out ledFrame
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            ledFrame[r][c] = 0;
        }
    }

    // 2) Internal accumulators: how many particles fell into each cell
    //    We'll count them here, then convert to brightness bins later.
    int counts[LED_ROWS][LED_COLS];
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            counts[r][c] = 0;
        }
    }

    // 3) For each particle, figure out which cell it belongs to
    // positions is [x0, y0, x1, y1, ...]
    for (size_t i = 0; i + 1 < positions.size(); i += 2) {
        double x = positions[i];
        double y = positions[i + 1];

        // Convert (x,y) to grid indices
        //   x in [-0.8..+0.8], so shift by +0.8 and divide by CELL_SIZE => [0..16)
        //   y in [0..0.9],     so divide by CELL_SIZE => [0..9)
        int x_index = static_cast<int>((x + SIM_W) / CELL_SIZE);
        int y_index = static_cast<int>(y / CELL_SIZE);

        // 4) Bounds check
        if (x_index >= 0 && x_index < LED_COLS &&
            y_index >= 0 && y_index < LED_ROWS)
        {
            // Accumulate the count
            counts[y_index][x_index]++;
        }
    }

    // 5) Convert counts to brightness via discrete bins:
    //    - We have VAR_INTENSITY bins (e.g., 5) for the entire range 0..255.
    //    - The top bin is 255, the bottom bin is 0.
    //    - Example: if VAR_INTENSITY=5, we have bins at brightness = 0, 64, 128, 191, 255
    //      (One way is dividing by (VAR_INTENSITY - 1)).
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            int countVal = counts[r][c];

            // You can clamp the count to VAR_INTENSITY-1 so it doesn't exceed the top bin
            if (countVal >= VAR_INTENSITY) {
                countVal = VAR_INTENSITY - 1;
            }

            // Map [0..(VAR_INTENSITY-1)] -> [0..255]
            // e.g. if countVal=4 and VAR_INTENSITY=5 => brightness=255
            // If VAR_INTENSITY=2 (like in this example), possible values are 0 or 255.
            int brightness = static_cast<int>(
                countVal * (255.0 / (VAR_INTENSITY - 1))
            );

            ledFrame[r][c] = static_cast<unsigned char>(brightness);
        }
    }
}

// -----------------------------------------------------------------------------
// Map SPH simulation coordinates to the LED matrix indices:
//   - SPHEngine uses x in [-SIM_W, SIM_W], y in [0, SIM_H].
//   - Our LED matrix has 16 columns (0..15) and 9 rows (0..8).
//
// Example approach (very simplistic):
//   col = int( ( (x - (-SIM_W)) / (2*SIM_W) ) * 16 )
//   row = int( ( y / SIM_H ) * 9 )
//   brightness = 255 if a particle is in that cell (or some blending).
// -----------------------------------------------------------------------------
// void mapParticlesToLEDs(const std::vector<double>& positions,
//                         unsigned char ledFrame[LED_ROWS][LED_COLS])
// {
//     // First, clear the frame
//     for (int r = 0; r < LED_ROWS; r++) {
//         for (int c = 0; c < LED_COLS; c++) {
//             ledFrame[r][c] = 0; // 0 brightness
//         }
//     }

//     // positions is [x0, y0, x1, y1, ...] in SPH coords
//     int numParticles = positions.size() / 2;

//     for (int i = 0; i < numParticles; i++) {
//         double x = positions[2*i + 0];
//         double y = positions[2*i + 1];

//         // Convert x,y to LED matrix indices
//         // Make sure to clamp indices so we don't go out of bounds.
//         double normX = (x - (-SIM_W)) / ( (SIM_W) - (-SIM_W) ); // in [0..1]
//         double normY = (y - BOTTOM )  / ( TOP - BOTTOM );       // in [0..1]
//         if (normX < 0) normX = 0; if (normX > 1) normX = 1;
//         if (normY < 0) normY = 0; if (normY > 1) normY = 1;

//         int c = (int)std::floor(normX * (LED_COLS));  // 0..16
//         int r = (int)std::floor(normY * (LED_ROWS));  // 0..9

//         if (c >= LED_COLS) c = LED_COLS - 1;
//         if (r >= LED_ROWS) r = LED_ROWS - 1;

//         // For simplicity, set brightness to 255 wherever there's a particle
//         ledFrame[r][c] = 255;
//     }
// }


// -----------------------------------------------------------------------------
// Write the 9×16 LED frame to Arduino using your ASCII protocol.
// We split columns 0..7 as "left half" and columns 8..15 as "right half".
// Format: "H,half,row,col,brightness\n"
// -----------------------------------------------------------------------------
void sendFrameToArduino(HANDLE hSerial, 
                        const unsigned char ledFrame[LED_ROWS][LED_COLS])
{
    DWORD bytesWritten;
    // For each row, for each column, decide half=0 if col<8, otherwise half=1
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            int half = (c < HALF_COLS) ? 0 : 1;
            int colInHalf = (half == 0) ? c : (c - HALF_COLS);
            int brightness = ledFrame[r][c];
            
            // Build the line: "H,half,r,colInHalf,brightness\n"
            std::string line = "H," +
                               std::to_string(half) + "," +
                               std::to_string(r) + "," +
                               std::to_string(colInHalf) + "," +
                               std::to_string(brightness) + "\n";

            // Write to the serial port
            WriteFile(hSerial, line.c_str(), DWORD(line.size()), &bytesWritten, NULL);
        }
    }
}


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    // 1) Open COM port (adjust to your Arduino port, e.g., "COM3")
    const char* portName = "COM3";
    HANDLE hSerial = openSerialPort(portName, CBR_115200);
    if (hSerial == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // 2) Create SPH simulation
    //    Let's say 200 particles, in a sub-region of [-0.5..0.5] x [0..0.5]
    Simulation sim(N, -SIM_W, SIM_W, BOTTOM, TOP);

    // 3) We'll store a 9×16 brightness frame in a 2D array
    static unsigned char ledFrame[LED_ROWS][LED_COLS];

    std::cout << "Starting simulation + sending frames to Arduino...\n"
              << "Press Ctrl+C or close window to terminate.\n";


    int frameCounter = 0;
    auto lastTime = std::chrono::steady_clock::now();
    double fps = 0.0;

    // 4) Main loop
    while (true) {
        auto loopStart = std::chrono::steady_clock::now();

        // a) Update simulation with dynamic gravity
        double dynamicAngle = G_ANG + frameCounter * M_PI / 100.0;
        frameCounter++;

        sim.update(G_MAG, dynamicAngle);

        // b) Get the visual positions
        std::vector<double> positions = sim.get_visual_positions();

        // c) Map to 9×16 brightness using the hash grid logic
        hashGrid(positions, ledFrame);

        // d) Send the frame to the Arduino
        sendFrameToArduino(hSerial, ledFrame);

        // e) Optional delay to control update speed
        // std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        // f) Pseudo FPS logging
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
        if (elapsedMs >= 1000) {
            fps = frameCount / (elapsedMs / 1000.0);
            frameCount = 0;
            lastTime = now;
        }

        std::cout << "\rFPS: " << fps << "   " << std::flush;
    }

    // (We never get here unless forcibly exited)
    CloseHandle(hSerial);
    return 0;
}
