# JUCE stand-in for a quick type-check (NOT JUCE)

`JuceHeader.h` declares just the slice of the JUCE 8 API that `Source/PluginEditor.*`, `Source/PluginProcessor.*` and `Source/UI/` use,
with the real signatures as far as they are known. It lets a machine without JUCE run `scripts/check_ui_syntax.sh` and catch typos, wrong
member names, bad overrides and type mismatches in the plugin glue in seconds.

It cannot catch a misremembered JUCE API, a changed JUCE signature, or anything about behaviour or looks. The real answer is always the
CI `plugin` job, which compiles against the real JUCE and loads the plugin in pluginval. If the stub and JUCE ever disagree, JUCE is right:
fix the stub. `Source/Offline/TakeWorker.cpp` is not covered (it needs more of JUCE than the stub declares).
