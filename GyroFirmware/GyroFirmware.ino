#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>


// -----------------------------------------------------------------------------
// Accelerometer Protocol
//   We'll send 1 header (0xFE) + 8 bytes for 2 floats = 9 bytes total
// -----------------------------------------------------------------------------
static const uint8_t ACCEL_HEADER = 0xFE;
static const int ACCEL_PACKET_SIZE = 9; // 1 + 8

// -----------------------------------------------------------------------------
// MPU6050 Accelerometer
// -----------------------------------------------------------------------------
Adafruit_MPU6050 mpu;

float tiltAngle = 0.0f;      // [0..360] degrees
float tiltMagnitude = 0.0f;  // [0..1]
float maxTiltMagnitude = 9.8f; // ~1g

// Compute tilt from X,Y
void calculateTilt(float x, float y, float z) {
  // angle
  tiltAngle = atan2(y, x) * 180.0f / M_PI;
  if (tiltAngle < 0) {
    tiltAngle += 360.0f;
  }
  // magnitude
  float xy = sqrtf(x*x + y*y);
  tiltMagnitude = xy / maxTiltMagnitude;
  if (tiltMagnitude > 1.0f) tiltMagnitude = 1.0f;
}

// -----------------------------------------------------------------------------
// Send Tilt Data (Binary): [0xFE] [4 bytes angle] [4 bytes magnitude] = 9 bytes
// -----------------------------------------------------------------------------
void sendTiltData(float angle, float mag) {
  // We'll pack floats into a union
  union FloatBytes {
    float f;
    uint8_t b[4];
  };

  FloatBytes angleData, magData;
  angleData.f = angle;
  magData.f   = mag;

  uint8_t packet[ACCEL_PACKET_SIZE];
  packet[0] = ACCEL_HEADER; // 0xFE

  // copy angle
  packet[1] = angleData.b[0];
  packet[2] = angleData.b[1];
  packet[3] = angleData.b[2];
  packet[4] = angleData.b[3];

  // copy magnitude
  packet[5] = magData.b[0];
  packet[6] = magData.b[1];
  packet[7] = magData.b[2];
  packet[8] = magData.b[3];

  Serial.write(packet, ACCEL_PACKET_SIZE);
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  
  // Init I2C and accelerometer
  Wire.begin(); 

  if (!mpu.begin()) {
    // If sensor not found, hang
    while (true) { delay(1000); }
  }
  Serial.println("MPU has began");
  for (int half = 0; half < HALVES; half++) {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        frame[half][y][x].x = x;
        frame[half][y][x].y = y;
        frame[half][y][x].a = 0;
      }
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  calculateTilt(accel.acceleration.x, accel.acceleration.y, accel.acceleration.z);
  sendTiltData(tiltAngle, tiltMagnitude);

}