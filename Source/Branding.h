#pragma once
// The name the user sees. JUCE-free so tests/branding_tests.cpp can check it against CMakeLists.txt and the UI sources.
//
// SkellyChoir is the product's name. These identifiers stay as they were on purpose, so sessions saved by earlier versions keep loading:
// the C++ class names, the APVTS state type "OMNICHOIR_FX_PARAMETERS", PLUGIN_CODE, PLUGIN_MANUFACTURER_CODE and BUNDLE_ID.
namespace branding {
constexpr const char* kProductName = "SkellyChoir";                         // CMake PRODUCT_NAME, the host's plugin list
constexpr const char* kTitle = "SKELLYCHOIR";                               // the header of the editor
constexpr const char* kSubtitle = "LIVE CHOIRIFICATION / LISTEN & RENDER";  // under the title
constexpr const char* kSettingsFolder = "SkellyChoir";                      // userApplicationDataDirectory / this: styles, window size, tour flag, theme
constexpr const char* kLegacySettingsFolder = "OmniChoir FX";               // copied once into the new folder, never deleted
} // namespace branding
