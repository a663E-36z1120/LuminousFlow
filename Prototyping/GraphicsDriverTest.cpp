#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

static const int HALVES = 2;
static const int ROWS = 9;
static const int COLS = 8;

int main() {
    // 1) Choose your COM port
    //    If the Arduino is on COM3, it’s just "COM3".
    //    For COM ports 10 or higher, use the prefix "\\\\.\\", e.g. "\\\\.\\COM10".
    const char* portName = "COM3";
    
    // 2) Open the serial port
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
        return 1;
    }

    // 3) Configure serial parameters
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to get current serial parameters." << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    // Example: 115200 baud, 8 data bits, no parity, 1 stop bit
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Failed to set serial parameters." << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    // 4) Optionally set timeouts
    COMMTIMEOUTS timeouts = {0};
    // No blocking on reads, 1 second total read timeout, etc.
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 1000;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "Failed to set timeouts." << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    // 5) Send a static frame in a loop until the program is terminated
    //    We'll just pick a static brightness pattern for demonstration.
    std::cout << "Sending static frame to " << portName 
              << " until the program is terminated." << std::endl;

    DWORD bytesWritten;

    // For demonstration, define some brightness. 
    // We'll do a simple gradient or constant brightness if you like:
    // e.g., brightness = 128 for all pixels.
    
    while (true) {
        // For each half, row, col
        for (int half = 0; half < HALVES; half++) {
            for (int y = 0; y < ROWS; y++) {
                for (int x = 0; x < COLS; x++) {
                    // Example: make a simple pattern
                    int brightness = 128; 
                    
                    // Build the line: "H,half,y,x,brightness\n"
                    std::string line = "H," 
                                     + std::to_string(half) + "," 
                                     + std::to_string(y) + "," 
                                     + std::to_string(x) + "," 
                                     + std::to_string(brightness) + "\n";

                    // Write to the serial port
                    if (!WriteFile(hSerial, line.c_str(), DWORD(line.size()), &bytesWritten, NULL)) {
                        std::cerr << "Error writing to " << portName << std::endl;
                    }
                }
            }
        }

        // Small delay before sending the next frame to avoid spamming the port too fast.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // We never actually get here unless the loop is broken or the program is killed.
    CloseHandle(hSerial);
    return 0;
}
