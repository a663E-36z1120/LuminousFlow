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

static const int LED_ROWS = 9;
static const int LED_COLS = 16;
static const int VAR_INTENSITY = 3; 
static const int HALF_COLS = 8;

static const double CELL_SIZE = 0.1;
static const int N = 250;

// For reading accelerometer data:
static const BYTE ACCEL_HEADER = 0xFE; 
static const int ACCEL_PACKET_SIZE = 9;  // 1 + 8 (two floats)

// -----------------------------------------------------------------------------
// Include Physics Engine
// -----------------------------------------------------------------------------
#include "SPHEngine.cpp"

// -----------------------------------------------------------------------------
// Helper to open and configure the serial port on Windows.
// -----------------------------------------------------------------------------
HANDLE openSerialPort(const char* portName, DWORD baudRate = CBR_115200) {
    HANDLE hSerial = CreateFileA(
        portName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening port " << portName << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    // Configure serial
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to get current serial parameters." << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to set serial parameters." << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

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
// Try to read a 9-byte packet: [0xFE][float angle][float magnitude]
// Returns true if we got a full packet. On success, outputs angle & magnitude.
// -----------------------------------------------------------------------------
bool readTiltData(HANDLE hSerial, float &angleDeg, float &magnitude) {
    COMSTAT stat;
    DWORD errors;
    ClearCommError(hSerial, &errors, &stat);

    DWORD bytesAvailable = stat.cbInQue;
    if (bytesAvailable < ACCEL_PACKET_SIZE) {
        return false; // Not enough data
    }

    // Read all available bytes (up to a safe max)
    const DWORD MAX_READ = 1024;
    uint8_t buffer[MAX_READ];
    DWORD bytesToRead = (bytesAvailable > MAX_READ) ? MAX_READ : bytesAvailable;
    DWORD bytesRead = 0;

    if (!ReadFile(hSerial, buffer, bytesToRead, &bytesRead, NULL) || bytesRead == 0) {
        return false;
    }

    // Search for the last valid packet (from end to start)
    for (int i = bytesRead - ACCEL_PACKET_SIZE; i >= 0; --i) {
        if (buffer[i] == ACCEL_HEADER) {
            // Found a possible packet
            union FloatBytes {
                float f;
                uint8_t b[4];
            } angleData, magData;

            for (int j = 0; j < 4; ++j)
                angleData.b[j] = buffer[i + 1 + j];
            for (int j = 0; j < 4; ++j)
                magData.b[j] = buffer[i + 5 + j];

            angleDeg = angleData.f;
            magnitude = magData.f;
            return true;
        }
    }

    return false; // No valid packet found
}

// -----------------------------------------------------------------------------
// HashGrid for converting SPH positions -> LED brightness
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

    // 2) Internal accumulators
    int counts[LED_ROWS][LED_COLS];
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            counts[r][c] = 0;
        }
    }

    // 3) For each particle
    for (size_t i = 0; i + 1 < positions.size(); i += 2) {
        double x = positions[i];
        double y = positions[i+1];

        int x_index = static_cast<int>((x + SIM_W) / CELL_SIZE);
        int y_index = static_cast<int>(y / CELL_SIZE);

        if (x_index >= 0 && x_index < LED_COLS &&
            y_index >= 0 && y_index < LED_ROWS)
        {
            counts[y_index][x_index]++;
        }
    }

    // 4) Convert counts to brightness
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            int countVal = counts[r][c];
            if (countVal >= VAR_INTENSITY) {
                countVal = VAR_INTENSITY - 1;
            }
            int brightness = static_cast<int>(countVal * (255.0 / (VAR_INTENSITY - 1)));
            ledFrame[r][c] = static_cast<unsigned char>(brightness);
        }
    }
}

// -----------------------------------------------------------------------------
// Send a 9×16 LED frame in binary: [0xFF] + 144 brightness bytes
// -----------------------------------------------------------------------------
void sendFrameToArduino(HANDLE hSerial, const unsigned char ledFrame[LED_ROWS][LED_COLS]) {
    static const BYTE HEADER = 0xFF;
    static const int TOTAL_BYTES = 1 + (LED_ROWS * LED_COLS); // 145

    unsigned char framePacket[TOTAL_BYTES];
    framePacket[0] = HEADER;

    int index = 1;
    for (int r = 0; r < LED_ROWS; r++) {
        for (int c = 0; c < LED_COLS; c++) {
            framePacket[index++] = ledFrame[r][c];
        }
    }

    DWORD bytesWritten;
    WriteFile(hSerial, framePacket, TOTAL_BYTES, &bytesWritten, NULL);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    // 1) Open COM ports
    // COM port for graphics
    const char* portNameGPU = "COM6";  // Adjust as needed
    HANDLE hSerialGPU = openSerialPort(portNameGPU, CBR_115200);
    if (hSerialGPU == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // COM port for gyro
    const char* portNameGyro = "COM7";  // Adjust as needed
    HANDLE hSerialGyro = openSerialPort(portNameGyro, CBR_115200);
    if (hSerialGyro == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // 2) Create SPH simulation
    Simulation sim(N, -SIM_W, SIM_W, BOTTOM, TOP);

    // 3) Frame buffer
    static unsigned char ledFrame[LED_ROWS][LED_COLS];

    std::cout << "Starting simulation + serial with Arduino(s)...\n";

    // We'll store the tilt angle (deg) and magnitude from Arduino
    float tiltAngleDeg  = 0.0f;
    float tiltMagnitude = 0.0f;

    // For FPS logging
    int frameCount = 0;
    auto lastTime = std::chrono::steady_clock::now();
    double fps = 0.0;

    while (true) {
        // a) Attempt to read accelerometer data (non-blocking)
        if (readTiltData(hSerialGyro, tiltAngleDeg, tiltMagnitude)) {
            // std::cout << "\rTiltAngle=" << tiltAngleDeg 
            // << " deg  TiltMag=" << tiltMagnitude 
            // << "     " << std::flush;
            // If we got new data, convert angle to radians, magnitude in [0..1]
            double angleRad = tiltAngleDeg * M_PI / 180.0;
            double dynGmag  = tiltMagnitude * G_MAG; 
            sim.update(G_MAG, angleRad);
        }
        
        //  else {
        //     // If no new data, we can keep last tilt or fallback to default
        //     sim.update(G_MAG, G_ANG);
        // }

        // b) Get updated particle positions
        std::vector<double> positions = sim.get_visual_positions();

        // c) Convert to 9×16 brightness
        hashGrid(positions, ledFrame);

        // d) Send to Arduino
        sendFrameToArduino(hSerialGPU, ledFrame);

        // e) FPS logging
        frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
        if (elapsedMs >= 1000) {
            fps = frameCount / (elapsedMs / 1000.0);
            frameCount = 0;
            lastTime = now;
        }
        std::cout << "\rFPS: " << fps << "   TiltAngle=" << tiltAngleDeg 
                  << " deg  TiltMag=" << tiltMagnitude << "     " << std::flush;
    }

    CloseHandle(hSerialGPU);
    CloseHandle(hSerialGyro);
    return 0;
}
