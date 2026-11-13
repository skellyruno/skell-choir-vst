// Skull logo data (Source/UI/SkullShape.h): valid, inside the unit square, symmetric, and recognisably a skull. JUCE-free.
#include "../Source/UI/SkullShape.h"
#include "test_support.h"
#include <algorithm>

using namespace skull;
static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

struct Box { float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f; };
static Box bounds(const Shape& s)
{
    Box b; const auto& v = s.v;
    if (s.kind == Kind::Ellipse) { b = { v[0] - v[2], v[1] - v[3], v[0] + v[2], v[1] + v[3] }; }
    else if (s.kind == Kind::RoundRect) { b = { v[0], v[1], v[0] + v[2], v[1] + v[3] }; }
    else for (int i = 0; i < s.count; i += 2) { b.x0 = std::min(b.x0, v[(size_t) i]); b.x1 = std::max(b.x1, v[(size_t) i]); b.y0 = std::min(b.y0, v[(size_t) i + 1]); b.y1 = std::max(b.y1, v[(size_t) i + 1]); }
    return b;
}
static float centreX(const Shape& s) { const Box b = bounds(s); return (b.x0 + b.x1) * .5f; }

int main()
{
    int body = 0, cuts = 0, glints = 0, eyes = 0;
    Box all; std::vector<float> cutCentres;
    for (int i = 0; i < kNumShapes; ++i)
    {
        const Shape& s = kShapes[i];
        const int need = s.kind == Kind::Ellipse ? 4 : s.kind == Kind::RoundRect ? 5 : -1;
        if (need > 0) CHECK(s.count == need, "shape %d: wrong number of values (%d, need %d)", i, s.count, need);
        else CHECK(s.count >= 6 && s.count <= 10 && s.count % 2 == 0, "shape %d: a polygon needs 3..5 points (%d values)", i, s.count);
        if (s.kind == Kind::Ellipse) CHECK(s.v[2] > 0 && s.v[3] > 0, "shape %d: ellipse radii must be positive", i);
        if (s.kind == Kind::RoundRect) CHECK(s.v[2] > 0 && s.v[3] > 0 && s.v[4] >= 0 && s.v[4] * 2 <= std::min(s.v[2], s.v[3]) + 1e-4f, "shape %d: bad rounded rectangle", i);
        const Box b = bounds(s);
        CHECK(b.x0 >= 0 && b.y0 >= 0 && b.x1 <= 1 && b.y1 <= 1, "shape %d leaves the unit square (%.2f,%.2f)-(%.2f,%.2f)", i, b.x0, b.y0, b.x1, b.y1);
        if (s.role == Role::Body) { ++body; all.x0 = std::min(all.x0, b.x0); all.y0 = std::min(all.y0, b.y0); all.x1 = std::max(all.x1, b.x1); all.y1 = std::max(all.y1, b.y1); }
        if (s.role == Role::Cut) { ++cuts; cutCentres.push_back(centreX(s)); if (s.kind == Kind::Ellipse) ++eyes; }
        if (s.role == Role::Glint) ++glints;
    }
    CHECK(body >= 2 && cuts >= 4 && eyes == 2 && glints >= 1, "a skull needs a body, two eye sockets, a nose and teeth (body %d, cuts %d, eyes %d)", body, cuts, eyes);
    // fills the square without touching the edges, so it cannot clip when drawn
    CHECK(all.x0 >= 0.02f && all.x1 <= 0.98f && all.y0 >= 0.02f && all.y1 <= 0.98f, "the body must keep a margin inside the unit square");
    CHECK(all.x1 - all.x0 >= 0.6f && all.y1 - all.y0 >= 0.7f, "the skull is too small inside its square");
    CHECK(near((all.x0 + all.x1) * .5f, .5f), "the skull is not centred");
    // symmetric about x = 0.5: every cut has a mirror image
    for (float c : cutCentres)
    {
        const float mirror = 1.f - c; bool found = false;
        for (float d : cutCentres) found = found || near(d, mirror);
        CHECK(found, "a cut at x=%.3f has no mirror image at x=%.3f", c, mirror);
    }
    // the eyes sit above the nose, the nose above the teeth
    float eyeY = 0, noseY = 0, teethY = 0;
    for (int i = 0; i < kNumShapes; ++i)
    {
        const Shape& s = kShapes[i]; if (s.role != Role::Cut) continue; const Box b = bounds(s);
        if (s.kind == Kind::Ellipse) eyeY = (b.y0 + b.y1) * .5f; else if (s.kind == Kind::Poly) noseY = (b.y0 + b.y1) * .5f; else teethY = (b.y0 + b.y1) * .5f;
    }
    CHECK(eyeY < noseY && noseY < teethY, "face layout is wrong (eyes %.2f, nose %.2f, teeth %.2f)", eyeY, noseY, teethY);
    // the cut-outs are small next to the body, or it stops reading as a solid logo
    float bodyArea = (all.x1 - all.x0) * (all.y1 - all.y0), cutArea = 0;
    for (int i = 0; i < kNumShapes; ++i) if (kShapes[i].role == Role::Cut) { const Box b = bounds(kShapes[i]); cutArea += (b.x1 - b.x0) * (b.y1 - b.y0); }
    CHECK(cutArea < 0.4f * bodyArea, "the cut-outs take %.0f%% of the skull", 100 * cutArea / bodyArea);
    std::printf("skull: %d shapes (%d body, %d cuts, %d glint); inside the unit square, centred, symmetric, face in the right order\n", kNumShapes, body, cuts, glints);
    return 0;
}
