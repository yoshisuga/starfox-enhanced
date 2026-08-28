#include "starfox/app/touch_controls.hpp"

#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace starfox::app {
namespace {

// The pads scale with the shorter screen edge so an iPhone and an iPad both
// land on thumb-sized targets rather than a layout tuned for one of them.
constexpr float kUnitFraction = 0.145F;
constexpr float kEdgeMargin = 0.55F;
constexpr float kDpadDeadzone = 0.28F;

constexpr std::array<input::ButtonMask, TouchControls::control_count>
    kControlButtons{
        0U, // the d-pad resolves to a direction pair from the touch position
        input::Button::a,
        input::Button::b,
        input::Button::x,
        input::Button::y,
        input::Button::left_shoulder,
        input::Button::right_shoulder,
        input::Button::select,
        input::Button::start,
    };

struct Rgba {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

constexpr Rgba kIdleFill{210U, 210U, 220U, 46U};
constexpr Rgba kActiveFill{255U, 210U, 90U, 130U};
constexpr Rgba kOutline{235U, 235U, 245U, 120U};

void set_color(SDL_Renderer* renderer, Rgba color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill_circle(SDL_Renderer* renderer, float cx, float cy, float radius,
    Rgba color) {
    constexpr int segments = 28;
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    const SDL_FColor tint{color.r / 255.0F, color.g / 255.0F,
        color.b / 255.0F, color.a / 255.0F};
    vertices.push_back(SDL_Vertex{SDL_FPoint{cx, cy}, tint, SDL_FPoint{}});
    for (int step = 0; step <= segments; ++step) {
        const auto angle = static_cast<float>(step) / segments
            * 2.0F * static_cast<float>(M_PI);
        vertices.push_back(SDL_Vertex{
            SDL_FPoint{cx + std::cos(angle) * radius,
                cy + std::sin(angle) * radius},
            tint, SDL_FPoint{}});
    }
    for (int step = 1; step <= segments; ++step) {
        indices.push_back(0);
        indices.push_back(step);
        indices.push_back(step + 1);
    }
    static_cast<void>(SDL_RenderGeometry(renderer, nullptr, vertices.data(),
        static_cast<int>(vertices.size()), indices.data(),
        static_cast<int>(indices.size())));
}

void draw_circle_outline(SDL_Renderer* renderer, float cx, float cy,
    float radius, Rgba color) {
    constexpr int segments = 48;
    set_color(renderer, color);
    for (int step = 0; step < segments; ++step) {
        const auto a0 = static_cast<float>(step) / segments
            * 2.0F * static_cast<float>(M_PI);
        const auto a1 = static_cast<float>(step + 1) / segments
            * 2.0F * static_cast<float>(M_PI);
        SDL_RenderLine(renderer,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius);
    }
}

} // namespace

void TouchControls::set_viewport(int width, int height) noexcept {
    width_ = static_cast<float>(std::max(width, 1));
    height_ = static_cast<float>(std::max(height, 1));
    unit_ = std::min(width_, height_) * kUnitFraction;
}

TouchControls::Region TouchControls::region(Control control) const noexcept {
    const auto margin = unit_ * kEdgeMargin;
    // The d-pad and face cluster sit against the bottom corners where thumbs
    // naturally rest; shoulders take the top corners, and the two system
    // buttons sit low and centred where they cannot be hit by accident.
    const auto dpad_radius = unit_ * 1.35F;
    const auto face_radius = unit_ * 0.52F;
    const auto face_spread = unit_ * 0.95F;
    const auto dpad_x = margin + dpad_radius;
    const auto dpad_y = height_ - margin - dpad_radius;
    const auto face_x = width_ - margin - face_spread - face_radius;
    const auto face_y = height_ - margin - face_spread - face_radius;

    switch (control) {
    case Control::dpad:
        return {dpad_x, dpad_y, dpad_radius, dpad_radius, true};
    case Control::a:
        return {face_x + face_spread, face_y, face_radius, face_radius, true};
    case Control::b:
        return {face_x, face_y + face_spread, face_radius, face_radius, true};
    case Control::x:
        return {face_x, face_y - face_spread, face_radius, face_radius, true};
    case Control::y:
        return {face_x - face_spread, face_y, face_radius, face_radius, true};
    case Control::left_shoulder:
        return {margin + unit_ * 0.9F, margin + unit_ * 0.42F,
            unit_ * 0.9F, unit_ * 0.42F, false};
    case Control::right_shoulder:
        return {width_ - margin - unit_ * 0.9F, margin + unit_ * 0.42F,
            unit_ * 0.9F, unit_ * 0.42F, false};
    // A 4:3 presentation fills the screen vertically, so the only space that
    // is not over the game is the two side bars. START and SELECT sit in the
    // gap between each shoulder pad and the thumb cluster below it.
    case Control::select:
        return {margin + unit_ * 0.9F, height_ * 0.45F,
            unit_ * 0.75F, unit_ * 0.32F, false};
    case Control::start:
        return {width_ - margin - unit_ * 0.9F, height_ * 0.45F,
            unit_ * 0.75F, unit_ * 0.32F, false};
    case Control::count:
        break;
    }
    return {};
}

input::ButtonMask TouchControls::mask_at(
    Control control, float x, float y) const noexcept {
    if (control == Control::count) return 0U;
    if (control != Control::dpad) {
        return kControlButtons[static_cast<std::size_t>(control)];
    }
    // Eight-way digital steering: a direction engages once the finger leaves
    // the centre deadzone, and diagonals hold both axes at once.
    const auto pad = region(Control::dpad);
    const auto dx = (x - pad.x) / pad.half_width;
    const auto dy = (y - pad.y) / pad.half_height;
    input::ButtonMask mask{};
    if (std::abs(dx) > kDpadDeadzone) {
        mask |= dx < 0.0F ? input::Button::left : input::Button::right;
    }
    if (std::abs(dy) > kDpadDeadzone) {
        mask |= dy < 0.0F ? input::Button::up : input::Button::down;
    }
    return mask;
}

TouchControls::Control TouchControls::control_at(
    float x, float y) const noexcept {
    for (std::size_t index = 0; index < control_count; ++index) {
        const auto control = static_cast<Control>(index);
        const auto area = region(control);
        if (area.half_width <= 0.0F) continue;
        const auto dx = x - area.x;
        const auto dy = y - area.y;
        if (area.circular) {
            // The d-pad claims a slightly wider ring than it paints so a thumb
            // that drifts off the edge keeps steering instead of cutting out.
            const auto reach = area.half_width
                * (control == Control::dpad ? 1.25F : 1.15F);
            if (dx * dx + dy * dy <= reach * reach) return control;
        } else if (std::abs(dx) <= area.half_width
                   && std::abs(dy) <= area.half_height) {
            return control;
        }
    }
    return Control::count;
}

void TouchControls::recompute_held() noexcept {
    input::ButtonMask mask{};
    for (const auto& touch : touches_) {
        if (touch.active) mask |= touch.mask;
    }
    held_ = mask;
}

bool TouchControls::handle_event(const SDL_Event& event) noexcept {
    if (!visible_) return false;
    if (event.type != SDL_EVENT_FINGER_DOWN
        && event.type != SDL_EVENT_FINGER_MOTION
        && event.type != SDL_EVENT_FINGER_UP
        && event.type != SDL_EVENT_FINGER_CANCELED) {
        return false;
    }
    // SDL reports touch positions normalised to the window.
    const auto x = event.tfinger.x * width_;
    const auto y = event.tfinger.y * height_;
    const auto finger = event.tfinger.fingerID;

    auto* slot = static_cast<Touch*>(nullptr);
    for (auto& touch : touches_) {
        if (touch.active && touch.finger == finger) {
            slot = &touch;
            break;
        }
    }

    if (event.type == SDL_EVENT_FINGER_DOWN) {
        if (slot == nullptr) {
            for (auto& touch : touches_) {
                if (!touch.active) {
                    slot = &touch;
                    break;
                }
            }
        }
        if (slot == nullptr) return false;
        const auto control = control_at(x, y);
        if (control == Control::count) return false;
        *slot = Touch{finger, control, mask_at(control, x, y), true};
        recompute_held();
        return true;
    }

    if (slot == nullptr) return false;

    if (event.type == SDL_EVENT_FINGER_MOTION) {
        // A finger stays owned by the control it started on. Only the d-pad
        // re-evaluates, so sliding a thumb changes direction without ever
        // sliding onto a face button by accident.
        if (slot->control == Control::dpad) {
            slot->mask = mask_at(Control::dpad, x, y);
            recompute_held();
        }
        return true;
    }

    *slot = Touch{};
    recompute_held();
    return true;
}

void TouchControls::release_all() noexcept {
    touches_.fill(Touch{});
    held_ = 0U;
}

void TouchControls::render(SDL_Renderer* renderer) const {
    if (!visible_) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (std::size_t index = 0; index < control_count; ++index) {
        const auto control = static_cast<Control>(index);
        const auto area = region(control);
        if (area.half_width <= 0.0F) continue;
        const auto button = kControlButtons[index];
        const auto engaged = control == Control::dpad
            ? (held_ & (input::Button::up | input::Button::down
                | input::Button::left | input::Button::right)) != 0U
            : (held_ & button) != 0U;
        const auto fill = engaged ? kActiveFill : kIdleFill;
        if (area.circular) {
            fill_circle(renderer, area.x, area.y, area.half_width, fill);
            draw_circle_outline(
                renderer, area.x, area.y, area.half_width, kOutline);
        } else {
            const SDL_FRect rect{area.x - area.half_width,
                area.y - area.half_height, area.half_width * 2.0F,
                area.half_height * 2.0F};
            set_color(renderer, fill);
            SDL_RenderFillRect(renderer, &rect);
            set_color(renderer, kOutline);
            SDL_RenderRect(renderer, &rect);
        }
    }
    // A crosshair marks the d-pad's rest position and its four cardinals.
    const auto pad = region(Control::dpad);
    set_color(renderer, kOutline);
    SDL_RenderLine(renderer, pad.x - pad.half_width * 0.45F, pad.y,
        pad.x + pad.half_width * 0.45F, pad.y);
    SDL_RenderLine(renderer, pad.x, pad.y - pad.half_height * 0.45F,
        pad.x, pad.y + pad.half_height * 0.45F);
}

} // namespace starfox::app
