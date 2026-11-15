// The opening curtain: closed and titled at the start, monotone opening, fully open and finished at the end, a click skips the hold. JUCE-free.
#include "../Source/UI/CurtainModel.h"
#include "test_support.h"

int main()
{
    using namespace curtain;
    long cases = 0; float prevOpen = -1.f;
    for (float ms = 0.f; ms <= kTotalMs + 500.f; ms += 1.f)
    {
        const State s = at(ms);
        CHECK(s.open >= 0.f && s.open <= 1.f, "open %.3f out of range at %.0f ms", s.open, ms);
        CHECK(s.title >= 0.f && s.title <= 1.f, "title %.3f out of range at %.0f ms", s.title, ms);
        CHECK(s.open >= prevOpen - 1e-6f, "the curtain closes again at %.0f ms", ms);
        prevOpen = s.open;
        CHECK(s.done == (ms >= kTotalMs), "done flag wrong at %.0f ms", ms);
        if (ms < kHoldMs) CHECK(s.open == 0.f, "opened too early at %.0f ms", ms);
        // never a gap while closed, never more than the full width when open
        for (float w : { 800.f, 1300.f, 2000.f }) { const float h = halfWidth(w, s.open); CHECK(h >= 0.f && h <= w * .5f + 1e-3f, "half width %.1f out of range", h); if (s.open == 0.f) CHECK(std::fabs(2.f * h - w) < 1e-3f, "gap in a closed curtain"); }
        ++cases;
    }
    CHECK(halfWidth(1000.f, 1.f) == 0.f && halfWidth(1000.f, 0.f) == 500.f, "a fully open curtain must clear the whole stage; a closed one must meet in the middle");
    CHECK(at(0.f).title == 0.f, "the title should fade in, not start visible");
    CHECK(at(kFadeInMs + 50.f).title > .99f, "the title should be fully visible during the hold");
    CHECK(at(kHoldMs).title > .99f && at(kHoldMs).open == 0.f, "the hold should end with a closed curtain and a visible title");
    CHECK(at(kTotalMs).open == 1.f && at(kTotalMs).title == 0.f && at(kTotalMs).done, "the end state is wrong");
    CHECK(at(kHoldMs + kOpenMs * .5f).open > .45f && at(kHoldMs + kOpenMs * .5f).open < .55f, "the opening is not centred in its time");
    CHECK(skip(0.f) == kHoldMs && skip(kHoldMs - 1.f) == kHoldMs, "a click in the hold should jump to the start of the opening");
    CHECK(skip(kHoldMs + 500.f) == kHoldMs + 500.f && skip(kTotalMs) == kTotalMs, "a click during the opening should change nothing");
    // the setting: names round-trip; unknown or damaged files give the default; each mode plays when it should
    for (int i = 0; i < kNumModes; ++i) CHECK(fromFileText(toFileText((Mode) i)) == (Mode) i, "mode %d does not round-trip", i);
    CHECK(fromFileText("") == Mode::Always && fromFileText("garbage") == Mode::Always && fromFileText("\xff\xfe\n") == Mode::Always, "damaged settings must give Always");
    CHECK(fromFileText("  Never \n") == Mode::Never && fromFileText("ONCE\r\n") == Mode::OncePerSession, "case and whitespace should not matter");
    CHECK(shouldPlay(Mode::Always, false) && shouldPlay(Mode::Always, true), "Always must always play");
    CHECK(shouldPlay(Mode::OncePerSession, false) && !shouldPlay(Mode::OncePerSession, true), "Once per session must play the first time only");
    CHECK(!shouldPlay(Mode::Never, false) && !shouldPlay(Mode::Never, true), "Never must never play");
    for (int i = 0; i < kNumModes; ++i) CHECK(std::string(modeName((Mode) i)).size() > 3 && std::string(modeName((Mode) i)).find_first_not_of(" abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos, "mode name %d must be plain ASCII letters", i);
    std::printf("curtain: %ld timeline steps, closed and titled for %.1f s, opens over %.1f s\n", cases, kHoldMs / 1000.f, kOpenMs / 1000.f);
    return 0;
}
