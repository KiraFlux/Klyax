#pragma once

template<typename T> struct LowFrequencyFilter {

private:

    const float alpha;
    const float one_minus_alpha{1.0f - alpha};

public:

    T filtered{};

    explicit LowFrequencyFilter(float alpha) noexcept:
        alpha{alpha} {}

    const T &calc(const T &x) noexcept {
        if (alpha == 1.0) {
            filtered = x;
        } else {
            filtered = filtered * one_minus_alpha + x * alpha;
        }
        return filtered;
    }
};

template<typename T> struct ComplementaryFilter {

private:

    const float alpha;
    const float one_minus_alpha{1.0f - alpha};

public:

    T filtered{};

    explicit ComplementaryFilter(float alpha) :
        alpha{alpha} {}

    const T &calc(T x, T dx, float dt) {
        T prediction = filtered + dx * dt;

        filtered = alpha * prediction + one_minus_alpha * x;
        return filtered;
    }
};
