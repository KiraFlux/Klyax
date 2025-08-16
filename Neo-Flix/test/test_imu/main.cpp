#include <ICM20948.h>
#include <SPI.h>


ICM20948 imu{SPI}; // no need to specify CS pin, the default pin is used automatically

void setup() {
    Serial.begin(115200);

    delay(1000);

    bool success = imu.begin();

    imu.setAccelRange(IMU::ACCEL_RANGE_4G);
    imu.setGyroRange(IMU::GYRO_RANGE_2000DPS);
    imu.setDLPF(IMU::DLPF_MAX);
    imu.setRate(IMU::RATE_1KHZ_APPROX);

    if (!success) {
        while (true) {
            Serial.println("Failed to initialize IMU");
            delay(5000);
        }
    }
}

void loop() {
    float gx, gy, gz, ax, ay, az, mx, my, mz;
    imu.waitForData(); // blockingly read the data, use IMU.read() for non-blocking read
    imu.getGyro(gx, gy, gz);
    imu.getAccel(ax, ay, az);
    imu.getMag(mx, my, mz);

    Serial.printf(
        "A[%+.2f %+.2f %+.2f]\t"
        "G[%+.2f %+.2f %+.2f]\t"
        "M[%+.2f %+.2f %+.2f]\n",
        ax, ay, az,
        gx, gy, gz,
        mx, my, mz
    );

    delay(50); // slow down the output
}