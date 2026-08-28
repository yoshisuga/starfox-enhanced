#pragma once

#include "starfox/input/input_latch.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace starfox::app {

// An on-screen SNES pad for touch-only devices. The overlay lives in window
// pixels rather than the framebuffer's logical space, so the pads sit in the
// letterbox bars beside a 4:3 presentation instead of covering the game.
class TouchControls {
public:
    // Every control the overlay can present, in draw order.
    enum class Control : std::uint8_t {
        dpad,
        a,
        b,
        x,
        y,
        left_shoulder,
        right_shoulder,
        select,
        start,
        count,
    };

    static constexpr std::size_t control_count =
        static_cast<std::size_t>(Control::count);

    void set_viewport(int width, int height) noexcept;

    // Consumes SDL touch events. Returns true when the event belonged to the
    // overlay, so the caller can leave mouse/gamepad handling untouched.
    [[nodiscard]] bool handle_event(const SDL_Event& event) noexcept;

    [[nodiscard]] input::ButtonMask held() const noexcept { return held_; }

    void render(SDL_Renderer* renderer) const;

    void set_visible(bool visible) noexcept { visible_ = visible; }
    [[nodiscard]] bool visible() const noexcept { return visible_; }

    // Drops every tracked finger, e.g. when the app loses focus mid-press.
    void release_all() noexcept;

private:
    struct Region {
        float x{};
        float y{};
        float half_width{};
        float half_height{};
        bool circular{};
    };

    struct Touch {
        SDL_FingerID finger{};
        Control control{Control::count};
        input::ButtonMask mask{};
        bool active{};
    };

    static constexpr std::size_t max_touches = 8U;

    [[nodiscard]] Region region(Control control) const noexcept;
    [[nodiscard]] input::ButtonMask mask_at(
        Control control, float x, float y) const noexcept;
    [[nodiscard]] Control control_at(float x, float y) const noexcept;
    void recompute_held() noexcept;

    std::array<Touch, max_touches> touches_{};
    input::ButtonMask held_{};
    float width_{1.0F};
    float height_{1.0F};
    float unit_{1.0F};
    bool visible_{true};
};

} // namespace starfox::app
