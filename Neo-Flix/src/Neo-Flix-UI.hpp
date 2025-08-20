#pragma once

#include "Text-UI.hpp"
#include "tools/PID.hpp"
#include "tools/Storage.hpp"


struct MainPage final : tui::Page, Singleton<MainPage> {
    friend struct Singleton<MainPage>;

    explicit MainPage() noexcept:
        Page{"Main"} {}
};

struct PidSettingsPage final : tui::Page, Singleton<PidSettingsPage> {
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
        i_limit{makeInput("I limit", pid_settings_storage.settings.i_limit)},
        pid_max_abs_output{makeInput("Max Out", pid_settings_storage.settings.output_abs_max)},
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
