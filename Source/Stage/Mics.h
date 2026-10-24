#pragma once
#include "Common.h"

namespace oc {

enum Pattern { Omni = 0, WideCardioid, Cardioid, Hypercardioid, Figure8 };
enum ArrayType { AB = 0, ORTF, XY, MS, Decca, Binaural };

constexpr double kPatternA[5] = { 1.0, 0.7, 0.5, 0.25, 0.0 };
constexpr double kDiffuse[5]  = { 1.0, 0.81, 0.58, 0.5, 0.58 };

struct Capsule
{
    V3 pos, dir { 0, 1, 0 };
    int pattern = Omni;
    double gainL = 1, gainR = 0;
    bool headShadow = false;
};

inline double patternGain(const Capsule& cap, double cosTheta)
{
    double a = kPatternA[cap.pattern];
    return a + (1 - a) * cosTheta;
}

struct MicArray
{
    int type = AB;
    V3 position;
    Capsule capsules[3];
    int numCapsules = 2;
};

struct MicSettings
{
    int mainType = AB;
    V3 mainPos { 0, 0, 3 };
    double mainSpacing = 0.6;
    V3 roomPos { 0, -8, 4.5 };
    double roomSpacing = 4;
    double closeDistance = 1.2, closeHeight = 2.1;
    double mainLevel = 1, closeLevel = 0.3548134, roomLevel = 0.5011872;
};

inline MicArray buildArray(int type, V3 position, double yawDeg, double spacing)
{
    const double yaw = yawDeg * kPi / 180;
    const V3 fwd { std::sin(yaw), std::cos(yaw), 0 }, right { std::cos(yaw), -std::sin(yaw), 0 };
    auto aim = [&](double deg) { double a = deg * kPi / 180; return normalised(fwd * std::cos(a) + right * std::sin(a)); };
    MicArray arr; arr.type = type; arr.position = position;
    Capsule &c0 = arr.capsules[0], &c1 = arr.capsules[1], &c2 = arr.capsules[2];
    arr.numCapsules = 2;
    auto pair = [&] { c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1; };
    switch (type)
    {
        case AB:
            c0.pos = position - right * (spacing * 0.5); c1.pos = position + right * (spacing * 0.5);
            c0.dir = c1.dir = fwd; c0.pattern = c1.pattern = Omni; pair(); break;
        case ORTF:
            c0.pos = position - right * 0.085; c1.pos = position + right * 0.085;
            c0.dir = aim(-55); c1.dir = aim(55); c0.pattern = c1.pattern = Cardioid; pair(); break;
        case XY:
            c0.pos = c1.pos = position;
            c0.dir = aim(-45); c1.dir = aim(45); c0.pattern = c1.pattern = Cardioid; pair(); break;
        case MS:   // mid cardioid forward, side figure-8 pointing left; L = M + S, R = M - S
            c0.pos = c1.pos = position;
            c0.dir = fwd; c0.pattern = Cardioid; c0.gainL = 0.7071; c0.gainR = 0.7071;
            c1.dir = right * -1; c1.pattern = Figure8; c1.gainL = 0.7071; c1.gainR = -0.7071; break;
        case Decca:
            arr.numCapsules = 3;
            c0.pos = position - right * std::max(0.5, spacing); c1.pos = position + right * std::max(0.5, spacing);
            c2.pos = position + fwd * 0.75;
            for (auto& c : arr.capsules) { c.dir = fwd; c.pattern = Omni; }
            pair(); c2.gainL = 0.5; c2.gainR = 0.5; break;
        default:   // Binaural
            c0.pos = position - right * 0.0875; c1.pos = position + right * 0.0875;
            c0.dir = right * -1; c1.dir = right; c0.pattern = c1.pattern = WideCardioid;
            c0.headShadow = c1.headShadow = true; pair(); break;
    }
    return arr;
}

} // namespace oc
