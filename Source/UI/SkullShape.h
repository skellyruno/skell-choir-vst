#pragma once
// The skull logo, as DATA: a list of simple shapes in a unit square (0,0 top left .. 1,1 bottom right, y down).
// JUCE-free on purpose: tools/render_skull_preview.py reads this very file to draw a preview PNG, so the picture can be looked at and
// iterated on without building the plugin, and tests/skull_shape_tests.cpp checks the data. SkullButton (PluginEditor.cpp) paints it.
//
// Roles: Body = filled with the theme's accent gradient. Cut = an eye, the nose, a gap between teeth: filled with the background.
//        Glint = a soft white highlight over the body.
// Keep one shape per line in exactly this form (the preview script parses it):
//   { Kind::Ellipse,   Role::..., 4, { cx, cy, rx, ry } }
//   { Kind::RoundRect, Role::..., 5, { x, y, w, h, radius } }
//   { Kind::Poly,      Role::..., 2*points, { x0, y0, x1, y1, ... } }      (up to 5 points)
#include <array>

namespace skull {

enum class Kind : int { Ellipse, RoundRect, Poly };
enum class Role : int { Body, Cut, Glint };
struct Shape { Kind kind; Role role; int count; std::array<float, 10> v; };

constexpr Shape kShapes[] = {
    // the skull
    { Kind::Ellipse,   Role::Body,  4, { .50f, .40f, .38f, .35f } },                                  // cranium
    { Kind::Poly,      Role::Body,  8, { .22f, .52f, .78f, .52f, .70f, .76f, .30f, .76f } },          // cheekbones
    { Kind::RoundRect, Role::Body,  5, { .31f, .70f, .38f, .22f, .05f } },                            // jaw
    // what is cut out of it
    { Kind::Ellipse,   Role::Cut,   4, { .345f, .45f, .105f, .115f } },                               // left eye
    { Kind::Ellipse,   Role::Cut,   4, { .655f, .45f, .105f, .115f } },                               // right eye
    { Kind::Poly,      Role::Cut,   6, { .50f, .575f, .455f, .675f, .545f, .675f } },                 // nose
    { Kind::RoundRect, Role::Cut,   5, { .366f, .765f, .014f, .125f, .006f } },                       // between the teeth
    { Kind::RoundRect, Role::Cut,   5, { .430f, .765f, .014f, .125f, .006f } },
    { Kind::RoundRect, Role::Cut,   5, { .493f, .765f, .014f, .125f, .006f } },
    { Kind::RoundRect, Role::Cut,   5, { .556f, .765f, .014f, .125f, .006f } },
    { Kind::RoundRect, Role::Cut,   5, { .620f, .765f, .014f, .125f, .006f } },
    // the shine
    { Kind::Ellipse,   Role::Glint, 4, { .37f, .20f, .12f, .05f } },
};
constexpr int kNumShapes = (int) (sizeof(kShapes) / sizeof(kShapes[0]));

} // namespace skull
