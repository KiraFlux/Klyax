#define Logger_level Logger_level_debug

#include "Text-UI.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include "espnow/Protocol.hpp"

#include "tools/Storage.hpp"
#include "tools/Logger.hpp"
#include "tools/time.hpp"

#include "DroneFrameDriver.hpp"
#include "EasyImu.hpp"
#include "tools/PID.hpp"


struct DroneControl final {

    /// Преобразование воздействия пульта в угловую скорость
    /// Радиан / секунду
    static constexpr float power_to_angular_velocity = 3.0f;

    /// ROLL
    /// [-1.0 .. 1.0]
    /// Канал пульта: right_x
    float roll_power;

    /// PITCH
    /// [-1.0 .. 1.0]
    /// Канал пульта: right_y
    float pitch_power;

    /// YAW
    /// [-1.0 .. 1.0]
    /// Канал пульта: left_x
    float yaw_power;

    /// THRUST
    /// [10.0 .. 1.0]
    /// Канал пульта: left_y
    float thrust;

    /// Включено
    bool armed;

    /// Интерпретировать pitch как угловую скорость
    inline float pitchVelocity() const { return pitch_power * power_to_angular_velocity; }

    /// Интерпретировать roll как угловую скорость
    inline float rollVelocity() const { return roll_power * power_to_angular_velocity; }

    /// Интерпретировать yaw как угловую скорость
    inline float yawVelocity() const { return yaw_power * power_to_angular_velocity; }
};

static DroneControl control{};

struct EspNowClient final {

    struct DualJoyControlPacket {
        float left_x;
        float left_y;
        float right_x;
        float right_y;

        bool mode_toggle;
    };

    enum MenuControlCode : uint8_t {
        Reload = 0x10,
        Click = 0x20,
        Left = 0x30,
        Right = 0x31,
        Up = 0x40,
        Down = 0x41
    };

    espnow::Mac target{0x78, 0x1c, 0x3c, 0xa4, 0x96, 0xdc};
    PacketTimeoutManager timeout_manager{200};

    bool init() const {
        Logger_info("init");

        const bool wifi_ok = WiFiClass::mode(WIFI_MODE_STA);
        if (not wifi_ok) {
            return false;
        }

        const auto init_result = espnow::Protocol::init();
        if (init_result.fail()) {
            Logger_error(rs::toString(init_result.error));
            return false;
        }

        const auto peer_result = espnow::Peer::add(target);
        if (peer_result.fail()) {
            Logger_error(rs::toString(peer_result.error));
            return false;
        }

        const auto handler_result = espnow::Protocol::instance().setReceiveHandler(EspNowClient::onReceive);
        if (handler_result.fail()) {
            Logger_error(rs::toString(handler_result.error));
            return false;
        }

        Logger_debug("success");
        return true;
    }

    static EspNowClient &instance() {
        static EspNowClient instance{};
        return instance;
    }

private:

    void onDualJoyControlPacket(const DualJoyControlPacket &packet) {
        timeout_manager.update();
        control.yaw_power = packet.left_x;
        control.thrust = packet.left_y;
        control.roll_power = packet.right_x;
        control.pitch_power = -packet.right_y;
        control.armed = packet.mode_toggle;
    }

    static void onMenuCodePacket(MenuControlCode code) {
        tui::PageManager::instance().addEvent(translateMenuCode(code));
    }

    static void onReceive(const espnow::Mac &mac, const void *data, rs::u8 size) {
        auto &self = instance();

        if (mac != self.target) {
            Logger_warn("got message from unknown device");
            return;
        }

        switch (size) {
            case sizeof(DualJoyControlPacket):
                self.onDualJoyControlPacket(*static_cast<const DualJoyControlPacket *>(data));
                return;

            case sizeof(MenuControlCode):
                EspNowClient::onMenuCodePacket(*static_cast<const MenuControlCode *>(data));
                return;

            default:
                Logger_warn("invalid packet size (%d B)", size);
                return;
        }
    }

    static tui::Event translateMenuCode(MenuControlCode code) {
        switch (code) {
            case Reload:
                return tui::Event::Update;
            case Click:
                return tui::Event::Click;
            case Left:
                return tui::Event::ChangeIncrement;
            case Right:
                return tui::Event::ChangeDecrement;
            case Up:
                return tui::Event::ElementPrevious;
            case Down:
                return tui::Event::ElementNext;

            default:
                Logger_warn("Invalid code: %d", code);
                return tui::Event::None;
        }
    }

    EspNowClient() = default;
};

static DroneFrameDriver frame_driver{
    .motors={
        Motor{12},
        Motor{13},
        Motor{14},
        Motor{15},
    }
};

static Storage<EasyImu::Settings> imu_storage{
    "imu", {
        .gyro_bias = {},
        .accel_bias = {-13.6719f, -17.8223f, -2.9297f},
        .accel_scale = {+0.0010f, +0.0010f, +0.0010f},
    }
};

static Storage<PID::Settings> pitch_or_roll_velocity_pid_storage{
    "pid-v-pr", PID::Settings{
        .p = 0.05f,
        .i = 0.01f,
        .d = 0.0002f,
        .i_limit = 0.1f,
        .output_min = -1.0f,
        .output_max = 1.0f,
    }
};

static Storage<PID::Settings> yaw_velocity_pid_storage{
    "pid-v-y", PID::Settings{
        .p = 0.03f,
        .i = 0.005f,
        .d = 0.0002f,
        .i_limit = 0.1f,
        .output_min = -1.0f,
        .output_max = 1.0f,
    }
};

static EasyImu imu{imu_storage.settings};

static void fatal() {
    Logger_fatal("Fatal Error. Reboot in 5s");
    delay(5000);
    ESP.restart();
}

void setupTui() {
    static tui::Page p1{"Page 1"};
    static tui::Page p2{"Page 2"};
    tui::PageManager::instance().bind(p1);

    {
        static tui::PageSetterButton to_page_2{p2};
        p1.add(to_page_2);

        static tui::Label label_1{"label 1"};
        p1.add(label_1);

        static tui::Label label_2{"label 2"};
        p1.add(label_2);

        static tui::Label label_3{"label 3"};
        p1.add(label_3);

        static tui::Label label_4{"label 4"};
        p1.add(label_4);

        static tui::Label label_5{"label 5"};
        p1.add(label_5);

        static tui::Label label_6{"label 6"};
        p1.add(label_6);

        static tui::Label label_7{"label 7"};
        p1.add(label_7);

        static tui::Label label_8{"label 8"};
        p1.add(label_8);

        static tui::Label label_9{"label 9"};
        p1.add(label_9);

        static tui::Label label_10{"label 10"};
        p1.add(label_10);
    }

    {
        static tui::PageSetterButton to_page_1{p1};
        p2.add(to_page_1);

        auto handler = [](const tui::Button &button) {
            Logger_debug("%s: click", button.label.string);
        };

        static tui::Button button_1{tui::Label{"button 1"}, handler};
        p2.add(button_1);

        static tui::Button button_2{tui::Label{"button 2"}, handler};
        p2.add(button_2);

        static float step = 0.1;
        static constexpr float step_step = 0.1;
        static tui::Labeled<tui::SpinBox<float>> spin_box_step{tui::Label{"spin 1"}, tui::SpinBox<float>{step, step_step}};
        p2.add(spin_box_step);

        static float value = 123.456;
        static tui::Labeled<tui::SpinBox<float>> spin_box_1{tui::Label{"spin 1"}, tui::SpinBox<float>{value, step}};
        p2.add(spin_box_1);
    }
}

void setup() {
    setupTui();
    delay(1000);

    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);

    Serial.begin(115200);
    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    frame_driver.init();

    if (not imu.init(GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_5)) {
        fatal();
    }

    pitch_or_roll_velocity_pid_storage.load();
    yaw_velocity_pid_storage.load();

    if (not imu_storage.load()) {
        Logger_warn("Failed to load IMU. Calib IMU..");

        imu.calibrateGyro(5000);
        imu_storage.save();
    }

    if (not EspNowClient::instance().init()) { fatal(); }

    digitalWrite(2, LOW);
    Logger_info("Start!");
}

void loop() {
    static Chronometer chronometer{};
    static auto &esp_now = EspNowClient::instance();
    static auto &page_manager = tui::PageManager::instance();

    static PID pitch_velocity_pid{
        pitch_or_roll_velocity_pid_storage.settings,
        0.1f,
    };

    static PID roll_velocity_pid{
        pitch_or_roll_velocity_pid_storage.settings,
        0.1f
    };

    static PID yaw_velocity_pid{
        yaw_velocity_pid_storage.settings,
        0.8f,
    };

    static LowFrequencyFilter<float> yaw_error_filter{0.4f};

    if (page_manager.pollEvents()) {
        const auto slice = page_manager.render();
        Logger_debug("Redraw page: %d chars", slice.len);
        espnow::Protocol::send(esp_now.target, slice.data, slice.len);
    }

    const auto dt = chronometer.calc();

    if (esp_now.timeout_manager.expired()) {
        control.armed = false;
    }

    if (control.armed) {
        const auto ned = imu.read(dt);

        constexpr float critical_angle = 70 * DEG_TO_RAD;

        if (std::abs(ned.pitch()) > critical_angle or std::abs(ned.roll()) > critical_angle) {
            Logger_warn("Critical roll/pitch. Disarming");
            control.armed = false;
            return;
        }

        const float roll = roll_velocity_pid.calc(
            control.rollVelocity() - ned.rollVelocity(),
            dt
        );

        const float pitch = pitch_velocity_pid.calc(
            control.pitchVelocity() - ned.pitchVelocity(),
            dt
        );

        const float yaw = yaw_velocity_pid.calc(
            yaw_error_filter.calc(control.yawVelocity() - ned.yawVelocity()),
            dt
        );

        frame_driver.mixin(
            control.thrust,
            roll,
            pitch,
            yaw
        );

    } else {
        control.thrust = 0;
        control.pitch_power = 0;
        control.yaw_power = 0;
        control.roll_power = 0;

        pitch_velocity_pid.reset();
        roll_velocity_pid.reset();
        yaw_velocity_pid.reset();
        yaw_error_filter.reset();

        frame_driver.disable();
    }

    delay(1);
}