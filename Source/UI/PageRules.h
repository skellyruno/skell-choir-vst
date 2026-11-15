#pragma once
// Which controls show in Easy and in Pro (JUCE-free, so the rule is tested natively).
//   easy:     a control that Easy mode shows (the big choir controls)
//   easyOnly: a control that exists only in Easy mode (like the one-button LISTEN)
// Easy shows the controls marked easy or easyOnly. Pro shows everything else: a control Easy already shows is NOT repeated in Pro,
// so the Pro page is less crowded (flip back to Easy for CHOIR SIZE, CHOIR MIX, OUTPUT and HARMONY).
namespace pagerules {

inline bool visibleIn(bool pro, bool easy, bool easyOnly) { return pro ? !(easy || easyOnly) : (easy || easyOnly); }

} // namespace pagerules
