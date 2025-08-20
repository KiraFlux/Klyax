#pragma once

#include <ela/vec3.hpp>

#include "Text-UI.hpp"
#include "tools/PID.hpp"
#include "tools/Storage.hpp"

#include "EasyImu.hpp"


namespace nfui {

template<typename T> struct Vec3Display final : tui::Widget {
    const ela::vec3<T> &vec;

    explicit Vec3Display(const ela::vec3<T> &vec) :
        vec{vec} {}

    bool onEvent(tui::Event event) override { return false; }

    void doRender(tui::TextStream &stream) const override {
        stream.printf("%.2f %.2f %.2f", vec.x, vec.y, vec.z);
    }
};

struct MainPage final : tui::Page, Singleton<MainPage> {
    friend struct Singleton<MainPage>;

    explicit MainPage() noexcept:
        Page{"Main"} {}
};

struct PidSettingsPage final : tui::Page {
    friend struct Singleton<PidSettingsPage>;

    using Input = tui::Labeled<tui::SpinBox<float>>;

    tui::Button save_button;
    Input p, i, d;
    Input i_limit;
    Input pid_max_abs_output;
    Input step;
    Input::Content::Scalar pid_step{0.1};
    const Input::Content::Scalar step_step{10.0f};

    explicit PidSettingsPage(Storage<PID::Settings> &pid_settings_storage) noexcept:
        Page{pid_settings_storage.key},
        save_button{
            "save", [&pid_settings_storage](const tui::Button &) {
                pid_settings_storage.save();
            }
        },
        p{makeInput("P", pid_settings_storage.settings.p)},
        i{makeInput("I", pid_settings_storage.settings.i)},
        d{makeInput("D", pid_settings_storage.settings.d)},
        i_limit{makeInput("I lim", pid_settings_storage.settings.i_limit)},
        pid_max_abs_output{makeInput("Max", pid_settings_storage.settings.output_abs_max)},
        step("step", Input::Content{pid_step, step_step, Input::Content::Mode::Geometric}) {
        MainPage::instance().link(*this);
        add(save_button);
        add(p);
        add(i);
        add(d);
        add(i_limit);
        add(pid_max_abs_output);
        add(step);
    }

private:

    Input makeInput(const char *label, Input::Content::Scalar &scalar) const noexcept {
        return Input{label, Input::Content{scalar, pid_step, Input::Content::Mode::ArithmeticPositiveOnly}};
    }
};

struct AccelCalibButton final : tui::Widget {
    EasyImu &imu;

    explicit AccelCalibButton(EasyImu &imu) :
        imu{imu} {}

    bool onEvent(tui::Event event) override {
        if (event != tui::Event::Click) { return false; }

        if (imu.isCalibratorActive()) {
            imu.resumeAccelCalib();
        } else {
            imu.startAccelCalib();
        }

        return true;
    }

    void doRender(tui::TextStream &stream) const override {
        stream.print(getText());
    }

private:

    const char *getText() const {
        static constexpr const char *orientations[EasyImu::AccelCalibrator::orientations_total]{
            "1 Level",
            "2 Upside Down",
            "3 Nose Up",
            "4 Nose Down",
            "5 Left Side",
            "6 Right Side",
        };

        if (imu.isCalibratorActive()) {
            return orientations[imu.getAccelCalibOrientation()];
        }

        return "[Calib Accel]";
    }
};

struct ImuPage final : tui::Page {

    tui::Button save;
    AccelCalibButton calib_accel;
    tui::Button calib_gyro;
    Vec3Display<float> accel_bias, accel_scale, gyro_bias;

    explicit ImuPage(Storage<EasyImu::Settings> &imu_storage, EasyImu &imu) :
        Page{imu_storage.key},
        save{
            "Save", [&imu_storage](const tui::Button &) {
                imu_storage.save();
            }
        },
        calib_accel{imu},
        calib_gyro{
            "Calib Gyro", [&imu](const tui::Button &) {
                imu.calibrateGyro(5000);
            }
        },
        accel_bias{imu_storage.settings.accel_bias},
        accel_scale{imu_storage.settings.accel_scale},
        gyro_bias{imu_storage.settings.gyro_bias} {
        MainPage::instance().link(*this);

        add(save);
        add(calib_gyro);
        add(calib_accel);
        add(accel_bias);
        add(accel_scale);
        add(gyro_bias);
    }
};

}