#pragma once
#include <cmath>
#include <vector>
namespace br {
enum class TouchState { Idle, Pressed, Dragging, Swiping, Pinching };
struct TouchPoint {
    long long id;
    float x, y;
};
enum class GestureType { None, Tap, DoubleTap, LongPress, Drag, Swipe, Pinch, Release };
struct Gesture {
    GestureType type = GestureType::None;
    float x = 0, y = 0, dx = 0, dy = 0, scale = 1, velocity = 0;
};
class Gestures {
    TouchState state_ = TouchState::Idle;
    float x_ = 0, y_ = 0, lastX_ = 0, lastY_ = 0, distance_ = 0, velocity_ = 0;
    double start_ = 0, last_ = 0, lastTap_ = -10;
    float tapX_ = 0, tapY_ = 0;
    bool pinched_ = false, longSent_ = false;
    long long firstId_ = 0;

  public:
    TouchState state() const { return state_; }
    Gesture update(const std::vector<TouchPoint>& points, double now, bool dragMode);
    void reset() {
        state_ = TouchState::Idle;
        pinched_ = false;
        velocity_ = 0;
    }
};
} // namespace br
