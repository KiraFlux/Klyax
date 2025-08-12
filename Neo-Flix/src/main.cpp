#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"

#include "Motor.hpp"
#include "ela/vec3.hpp"


struct [[gnu::packed]] DualJoyPacket {
    float x, y, a;
};

static constexpr espnow::Mac target = {0x78, 0x1c, 0x3c, 0xa4, 0x96, 0xdc};

constexpr auto joy_packet_timeout_ms = 500;

static Motor motors[4] = {
    Motor(12), // M0 - Back Left
    Motor(13), // M1 - Back Right
    Motor(14), // M2 - Front Left
    Motor(15), // M3 - Front Right
};

bool running = false;

uint32_t stop_time_ms = 0;

ela::vec3f vel;

void f(const espnow::Mac &mac, const void *data, rs::u8 size) {
    if (size != sizeof(DualJoyPacket)) {
        return;
    }

    stop_time_ms = millis() + joy_packet_timeout_ms;

    const auto &packet = *static_cast<const DualJoyPacket *>(data);
    vel.x = constrain(packet.x, -1, 1);
    vel.y = constrain(packet.y, -1, 1);
    vel.z = constrain(packet.a, 0, 1);
}

void setup() {
    for (auto &m: motors) {
        m.init();
    }

    Serial.begin(115200);

    {
        WiFiClass::mode(WIFI_MODE_STA);

        const auto result = espnow::Protocol::init();

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }
    }

    {
        const auto result = espnow::Peer::add(target);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }
    }

    {
        const auto result = espnow::Protocol::instance().setReceiveHandler(f);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }
    }

    Serial.println("Start!");
}

void loop() {
    running = millis() < stop_time_ms;

    if (not running) {
        vel.x = 0;
        vel.y = 0;
        vel.z = 0;

        for (auto &m: motors) {
            m.write(0);
        }

        return;
    }

    motors[0].write(vel.y + vel.x + vel.z); // Задний левый
    motors[1].write(vel.y - vel.x + vel.z); // Задний правый
    motors[2].write(vel.y + vel.x + vel.z); // Передний левый
    motors[3].write(vel.y - vel.x + vel.z); // Передний правый

//    Serial.printf("%.3f\t%.3f\t%.3f\n", vel.x, vel.y, vel.z);
}
