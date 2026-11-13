#pragma once
// Stage scenes: the picture behind the singers follows the stage preset (room) that was last applied. JUCE-free so the mapping,
// the fade and the picture files can be tested natively (tests/scene_tests.cpp). Scene index == stage preset index (StagePresets.h).
// Default and Cathedral share the original photo. The files are in Assets/ and listed in CMakeLists.txt (juce_add_binary_data);
// tools/make_scene_images.py makes the eight new ones.
namespace scenes {

constexpr int kNumScenes = 10;
constexpr int kFadeMs = 400;
constexpr int kImageWidth = 1200, kImageHeight = 530;

struct Scene { const char* presetName; const char* file; };
constexpr Scene kScenes[kNumScenes] = {
    { "Default",         "Assets/cathedral_stage.jpg" },
    { "Cathedral",       "Assets/cathedral_stage.jpg" },
    { "Concert Hall",    "Assets/Scenes/scene_concert_hall.jpg" },
    { "Studio Booth",    "Assets/Scenes/scene_studio_booth.jpg" },
    { "Chapel",          "Assets/Scenes/scene_chapel.jpg" },
    { "Gospel Stage",    "Assets/Scenes/scene_gospel_stage.jpg" },
    { "Stadium Anthem",  "Assets/Scenes/scene_stadium_anthem.jpg" },
    { "Headphone Choir", "Assets/Scenes/scene_headphone_choir.jpg" },
    { "Graveyard",       "Assets/Scenes/scene_graveyard.jpg" },
    { "Crypt",           "Assets/Scenes/scene_crypt.jpg" },
};

inline int clampScene(int i) { return i < 0 || i >= kNumScenes ? 0 : i; }
// the picture's file for a scene; two scenes with the same file share one picture (no fade needed between them)
inline bool samePicture(int a, int b) { const char* x = kScenes[clampScene(a)].file; const char* y = kScenes[clampScene(b)].file; while (*x && *x == *y) { ++x; ++y; } return *x == *y; }
// 0 at the start of a crossfade, 1 at its end (smoothstep over kFadeMs)
inline float fadeProgress(double elapsedMs)
{
    const double t = elapsedMs <= 0 ? 0.0 : elapsedMs >= kFadeMs ? 1.0 : elapsedMs / kFadeMs;
    return (float) (t * t * (3.0 - 2.0 * t));
}

} // namespace scenes
