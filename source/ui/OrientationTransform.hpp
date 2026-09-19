#pragma once
#include <SDL.h>
namespace br {
struct Point {
    float x, y;
};
// The entire logical surface is rotated clockwise. Touch uses its exact inverse.
class OrientationTransform {
    bool portrait_;

  public:
    explicit OrientationTransform(int orientation) : portrait_(orientation != 0) {}
    int width() const { return portrait_ ? 720 : 1280; }
    int height() const { return portrait_ ? 1280 : 720; }
    Point logicalToPhysical(Point p) const { return portrait_ ? Point{1280 - p.y, p.x} : p; }
    Point physicalToLogical(Point p) const { return portrait_ ? Point{p.y, 1280 - p.x} : p; }
    Point physicalVectorToLogical(Point p) const { return portrait_ ? Point{p.y, -p.x} : p; }
    SDL_Rect presentationRect() const {
        return portrait_ ? SDL_Rect{280, -280, 720, 1280} : SDL_Rect{0, 0, 1280, 720};
    }
    double angle() const { return portrait_ ? 90.0 : 0.0; }
};
} // namespace br
