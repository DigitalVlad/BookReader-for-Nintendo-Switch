#include "ui/Gestures.hpp"
#include <algorithm>
namespace br {
Gesture Gestures::update(const std::vector<TouchPoint>& points, double now, bool dragMode) {
    Gesture g;
    if (points.size() >= 2) {
        float d = std::hypot(points[0].x - points[1].x, points[0].y - points[1].y);
        if (state_ != TouchState::Pinching) {
            state_ = TouchState::Pinching;
            distance_ = std::max(d, 1.f);
            pinched_ = true;
            velocity_ = 0;
            lastTap_ = -10;
        }
        g.type = GestureType::Pinch;
        g.scale = d / std::max(distance_, 1.f);
        distance_ = d;
        g.x = (points[0].x + points[1].x) * .5f;
        g.y = (points[0].y + points[1].y) * .5f;
        return g;
    }
    if (points.empty()) {
        if (state_ == TouchState::Idle)
            return g;
        g.x = lastX_;
        g.y = lastY_;
        g.dx = lastX_ - x_;
        g.dy = lastY_ - y_;
        g.velocity = velocity_;
        if (!pinched_ && !longSent_ && state_ == TouchState::Pressed) {
            if (now - start_ >= .6) {
                g.type = GestureType::LongPress;
            } else if (now - lastTap_ < .3 && std::hypot(g.x - tapX_, g.y - tapY_) < 32) {
                g.type = GestureType::DoubleTap;
                lastTap_ = -10;
            } else {
                g.type = GestureType::Tap;
                lastTap_ = now;
                tapX_ = g.x;
                tapY_ = g.y;
            }
        } else if (!pinched_ && !longSent_ && state_ == TouchState::Swiping &&
                   std::abs(g.dx) > 60 && std::abs(g.dx) > std::abs(g.dy) * 1.3f)
            g.type = GestureType::Swipe;
        else
            g.type = GestureType::Release;
        state_ = TouchState::Idle;
        pinched_ = false;
        return g;
    }
    auto p = points[0];
    if (state_ == TouchState::Idle) {
        x_ = lastX_ = p.x;
        y_ = lastY_ = p.y;
        firstId_ = p.id;
        start_ = last_ = now;
        state_ = TouchState::Pressed;
        longSent_ = false;
        velocity_ = 0;
        return g;
    }
    if (pinched_)
        return g; // A pinch consumes the entire contact sequence.
    if (p.id != firstId_) {
        pinched_ = true;
        return g;
    }
    float dx = p.x - lastX_, dy = p.y - lastY_, totalX = p.x - x_, totalY = p.y - y_;
    double dt = std::max(.001, now - last_);
    g.x = p.x;
    g.y = p.y;
    g.dx = dx;
    g.dy = dy;
    if (state_ == TouchState::Pressed && std::hypot(totalX, totalY) > 12) {
        state_ = dragMode || std::abs(totalY) >= std::abs(totalX) ? TouchState::Dragging
                                                                  : TouchState::Swiping;
        lastTap_ = -10;
    }
    if (state_ == TouchState::Pressed && now - start_ >= .6 && !longSent_) {
        g.type = GestureType::LongPress;
        longSent_ = true;
    }
    if (state_ == TouchState::Dragging && !longSent_) {
        g.type = GestureType::Drag;
        velocity_ = std::clamp(.65f * velocity_ + .35f * float(dy / dt), -2500.f, 2500.f);
    }
    lastX_ = p.x;
    lastY_ = p.y;
    last_ = now;
    return g;
}
} // namespace br
