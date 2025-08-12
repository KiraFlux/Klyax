#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"

#include "Motor.hpp"
#include "ela/vec3.hpp"


#define LOG(x) Serial.println(x)

struct DroneControl {
    /// left_x Поворот Z [-1.0 .. 1.0]
    float yaw;

    /// left_y Тяга [0.0 .. 1.0]
    float throttle;

    /// right_x поворот бок влево-вправо [-1.0 .. 1.0]
    float roll;

    /// right_y поворот вперед-назад [-1.0 .. 1.0]
    float pitch;


    bool f1, f2, f3, f4; // Дополнительные функции от пульта
};

DroneControl control;

static constexpr espnow::Mac target = {0x78, 0x1c, 0x3c, 0xa4, 0x96, 0xdc};

constexpr auto shutdown_timeout_ms = 1000;

uint32_t shutdown_time_ms = 0;

// Конфигурация моторов (X-расположение)
static Motor motors[4] = {
    Motor(12), // M0 - Задний левый
    Motor(13), // M1 - Задний правый
    Motor(14), // M2 - Передний левый
    Motor(15), // M3 - Передний правый
};

void onEspNowMessage(const espnow::Mac &mac, const void *data, rs::u8 size) {
    if (size != sizeof(DroneControl)) {
        LOG("Packet err");
        return;
    }

    control = *reinterpret_cast<const DroneControl *>(data);
    control.throttle = constrain(control.throttle, 0.0, 1.0);
    shutdown_time_ms = millis() + shutdown_timeout_ms;
}

void setup() {
    Serial.begin(115200);
    LOG("Init...");

    for (auto &m: motors) {
        m.init();
        m.write(0); // Инициализация с нулевой мощностью
    }

    LOG("Init Done");

    WiFiClass::mode(WIFI_MODE_STA);
    const auto init_result = espnow::Protocol::init();
    if (init_result.fail()) {
        LOG(rs::toString(init_result.error));
    }

    const auto peer_result = espnow::Peer::add(target);
    if (peer_result.fail()) {
        LOG(rs::toString(peer_result.error));
    }

    const auto handler_result = espnow::Protocol::instance().setReceiveHandler(onEspNowMessage);
    if (handler_result.fail()) {
        LOG(rs::toString(handler_result.error));
    }

    // Инициализация времени безопасности
    shutdown_time_ms = millis() + shutdown_timeout_ms;
    LOG("Start!");
}

void sendMotors() {
    // Распаковка управления
    const float throttle = control.throttle;
    const float yaw = control.yaw;
    const float pitch = control.pitch;
    const float roll = control.roll;

    // Микширование каналов для X-конфигурации
    motors[0].write(throttle + yaw + pitch + roll);  // Задний левый
    motors[1].write(throttle - yaw + pitch - roll);  // Задний правый
    motors[2].write(throttle + yaw - pitch + roll);  // Передний левый
    motors[3].write(throttle - yaw - pitch - roll);  // Передний правый
}

void safetyCheck() {
    if (millis() > shutdown_time_ms) {
        // Плавное снижение тяги
        control.throttle = max(0.0f, control.throttle - 0.02f);

        // Сброс управления
        control.yaw = 0;
        control.pitch = 0;
        control.roll = 0;
    }
}

void loop() {
    constexpr auto loop_hz = 100;
    constexpr auto ms_delay = 1000 / loop_hz;

    safetyCheck();
    sendMotors();

    delay(ms_delay);
}