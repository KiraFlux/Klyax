#define Logger_level Logger_level_debug

#include "Neo-Flix-UI.hpp"

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

struct EspNowClient final : Singleton<EspNowClient> {
    friend struct Singleton<EspNowClient>;

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

private:

    void onDualJoyControlPacket(const DualJoyControlPacket &packet) {
        timeout_manager.update();

        control.yaw_power = packet.left_x;
        control.thrust = packet.left_y;

        control.roll_power = packet.right_x;
        control.pitch_power = packet.right_y;

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
        .accel_bias = {},
        .accel_scale = {0.0010f, 0.0010f, 0.0010f},
    }
};

struct Behavior {
    virtual void interpret(
        const DroneControl &c,
        float dt,
        const EasyImu::FLU &flu
    ) = 0;

    virtual void onDisarm() = 0;
};

struct BehaviorManager final : Singleton<BehaviorManager> {
    friend struct Singleton<BehaviorManager>;

private:

    Behavior *active_behavior{nullptr};

public:

    void bind(Behavior &behavior) {
        active_behavior = &behavior;
    }

    bool isActive(const Behavior &behavior) const {
        return &behavior == active_behavior;
    }

    void interpret(
        const DroneControl &c,
        float dt,
        const EasyImu::FLU &flu
    ) const {
        if (active_behavior == nullptr) { return; }
        active_behavior->interpret(c, dt, flu);
    }

    void onDisarm() const {
        if (active_behavior == nullptr) { return; }
        active_behavior->onDisarm();
    }
};

struct ManualModeBehavior final : Behavior, Singleton<ManualModeBehavior> {
    friend struct Singleton<ManualModeBehavior>;

    void interpret(const DroneControl &c, float dt, const EasyImu::FLU &flu) override {
        frame_driver.mixin(
            c.thrust,
            c.roll_power,
            c.pitch_power,
            c.yaw_power
        );
    }

    void onDisarm() override {}
};

struct AcrobaticModeBehavior final : Behavior, Singleton<AcrobaticModeBehavior> {
    friend struct Singleton<AcrobaticModeBehavior>;

    Storage<PID::Settings> pitch_or_roll_velocity_pid_storage{
        "pid-v-pr", PID::Settings{
            .p = 0.05f,
            .i = 0.01f,
            .d = 0.0002f,
            .i_limit = 0.1f,
            .output_abs_max = 1.0f,
        }
    };

    Storage<PID::Settings> yaw_velocity_pid_storage{
        "pid-v-y", PID::Settings{
            .p = 0.03f,
            .i = 0.005f,
            .d = 0.0002f,
            .i_limit = 0.1f,
            .output_abs_max = 1.0f,
        }
    };

    PID pitch_velocity_pid{
        pitch_or_roll_velocity_pid_storage.settings,
        0.2f,
    };

    PID roll_velocity_pid{
        pitch_or_roll_velocity_pid_storage.settings,
        0.2f
    };

    PID yaw_velocity_pid{
        yaw_velocity_pid_storage.settings,
        0.8f,
    };

    LowFrequencyFilter<float> yaw_error_filter{0.4f};

    void init() {
        pitch_or_roll_velocity_pid_storage.load();
        yaw_velocity_pid_storage.load();
    }

    void interpret(const DroneControl &c, float dt, const EasyImu::FLU &flu) override {
        const float roll = roll_velocity_pid.calc(
            control.rollVelocity() - flu.rollVelocity(),
            dt
        );

        const float pitch = pitch_velocity_pid.calc(
            control.pitchVelocity() - flu.pitchVelocity(),
            dt
        );

        const float yaw = -yaw_velocity_pid.calc(
            yaw_error_filter.calc(control.yawVelocity() - flu.yawVelocity()),
            dt
        );

        frame_driver.mixin(
            control.thrust,
            roll,
            pitch,
            yaw
        );
    }

    void onDisarm() override {
        pitch_velocity_pid.reset();
        roll_velocity_pid.reset();
        yaw_velocity_pid.reset();
        yaw_error_filter.reset();
    }
};

static EasyImu imu{imu_storage.settings};

static void fatal() {
    Logger_fatal("Fatal Error. Reboot in 5s");
    delay(5000);
    ESP.restart();
}

void setupTui() {
    static auto &acrobatic_mode_behavior = AcrobaticModeBehavior::instance();
    static nfui::PidSettingsPage pitch_or_roll_vel_page{acrobatic_mode_behavior.pitch_or_roll_velocity_pid_storage};
    static nfui::PidSettingsPage yaw_vel_page{acrobatic_mode_behavior.yaw_velocity_pid_storage};
    static nfui::ImuPage imu_page{imu_storage, imu};

    auto &main_page = nfui::MainPage::instance();
    static tui::Button switch_mode("m", [](tui::Button &button) {
        static auto &behavior_manager = BehaviorManager::instance();
        if (behavior_manager.isActive(acrobatic_mode_behavior)) {
            behavior_manager.bind(ManualModeBehavior::instance());
            button.label = "Manual";
        } else {
            behavior_manager.bind(acrobatic_mode_behavior);
            button.label = "Acrobatic";
        }
    });
    main_page.add(switch_mode);

    tui::PageManager::instance().bind(main_page);
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

    imu_storage.load();
    AcrobaticModeBehavior::instance().init();

    if (not EspNowClient::instance().init()) { fatal(); }

    digitalWrite(2, LOW);
    Logger_info("Start!");

    BehaviorManager::instance().bind(AcrobaticModeBehavior::instance());
}

void loop() {
    static Chronometer chronometer{};
    static auto &esp_now = EspNowClient::instance();
    static auto &page_manager = tui::PageManager::instance();
    static auto &behavior_manager = BehaviorManager::instance();

    delay(1);

    if (imu.isCalibratingAccel()) {
        const bool state_changed = imu.updateAccelCalib();
        if (state_changed) {
            page_manager.addEvent(tui::Event::Update);
        }
    }

    if (page_manager.pollEvents()) {
        const auto slice = page_manager.render();
        espnow::Protocol::send(esp_now.target, slice.data, slice.len);
    }

    if (esp_now.timeout_manager.expired()) {
        control.armed = false;
    }

    const auto dt = chronometer.calc();

    if (control.armed) {
        const auto flu = imu.read(dt);

        constexpr float critical_angle = 60 * DEG_TO_RAD;

        if (std::abs(flu.pitch()) > critical_angle or std::abs(flu.roll()) > critical_angle) {
            Logger_warn("Critical roll/pitch. Disarming");
            control.armed = false;
            return;
        }

        behavior_manager.interpret(control, dt, flu);

    } else {
        behavior_manager.onDisarm();
        control.thrust = 0;
        control.pitch_power = 0;
        control.yaw_power = 0;
        control.roll_power = 0;
        frame_driver.disable();
    }

}