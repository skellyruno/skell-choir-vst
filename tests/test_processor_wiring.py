"""Catch commented-out or missing host parameter wiring in readParams().

This is a source-level guard; it does not replace a compiled JUCE/VST3 host test.
"""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "Source" / "PluginProcessor.cpp"

class ProcessorWiringTests(unittest.TestCase):
    def test_existing_vibrato_and_classic_stage_reach_dsp(self):
        code = SOURCE.read_text(encoding="utf-8")
        self.assertIn("choir::FXParams OmniChoirAudioProcessor::readParams() const", code)
        body = code.split("choir::FXParams OmniChoirAudioProcessor::readParams() const", 1)[1].split(
            "void OmniChoirAudioProcessor::processBlock", 1
        )[0]
        # Strip line comments; an assignment hidden after // is not executable C++.
        body = re.sub(r"//[^\n]*", "", body)
        for parameter in ("vibrato", "classicStage"):
            with self.subTest(parameter=parameter):
                self.assertRegex(body, rf"\bp\.{parameter}\s*=\s*read\(raw\.{parameter}\)")

    def test_legacy_and_new_pitch_algorithms_independent(self):
        code = re.sub(r"//[^\n]*", "", SOURCE.read_text(encoding="utf-8"))
        self.assertRegex(code, r"p\.lockGrains\s*=\s*read\(raw\.grainLock\)")
        self.assertRegex(code, r"p\.pitchSync\s*=\s*read\(raw\.pitchSync\)")
        self.assertNotRegex(code, r"p\.pitchSync\s*=\s*read\(raw\.grainLock\)")
        self.assertIn('raw.pitchSync=get("pitchSync")', code)
        self.assertIn('ParameterID{"pitchSync",1},"Pitch Sync",false', code)
        layout = code.split('::createLayout() {', 1)[1].split('return layout;', 1)[0]
        tail = layout.index('addFloat(layout,"pulseDepth"')
        for new in ('stageSpread', 'toneAmount', 'phraseBreath', 'vibeSpread', 'pitchSync', 'tone'):
            with self.subTest(parameter=new):
                self.assertGreater(layout.index('"' + new + '"'), tail)
        self.assertIn('if(version<5)', code)

if __name__ == "__main__":
    unittest.main()
