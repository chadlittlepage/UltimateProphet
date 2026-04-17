#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * Prophet-5 Envelope Generator
 *
 * Models both envelope types used in the Prophet-5:
 *
 * Rev 3 (CEM 3310 / Curtis):
 *   - Exponential RC charge curves
 *   - Attack overshoots to ~1.37x (capacitor charges toward voltage above threshold)
 *   - Decay/Release are exponential decays
 *   - "More of a curve" — snappy, punchy character
 *
 * Rev 1/2 (SSM 2050):
 *   - Nearly linear envelope shapes
 *   - Attack is more linear ramp
 *   - Decay/Release are more linear
 *   - "Very flat, almost linear" — smoother, more gradual
 *
 * The Rev switch changes the envelope shape to match.
 */
class CEM3310Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate);

    void setAttack(float seconds);      // 0.001 - 10.0
    void setDecay(float seconds);       // 0.001 - 10.0
    void setSustain(float level);       // 0.0 - 1.0
    void setRelease(float seconds);     // 0.001 - 10.0

    // Rev mode: 0 = Rev 1/2 (linear SSM), 1 = Rev 3 (curved CEM)
    void setRevMode(int rev) { revMode = rev; }

    // Release switch: when OFF, release is always fast regardless of knob
    void setReleaseEnabled(bool on) { releaseEnabled = on; }

    void noteOn();
    void noteOff();

    float process();
    float getCurrentValue() const { return currentValue; }
    Stage getStage() const { return stage; }
    bool isActive() const { return stage != Stage::Idle; }

private:
    float calcCoeff(float timeSeconds) const;

    double sampleRate = 44100.0;

    float attackCoeff = 0.0f;
    float decayCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float sustainLevel = 0.7f;

    float currentValue = 0.0f;
    Stage stage = Stage::Idle;
    int revMode = 1;            // default Rev 3
    bool releaseEnabled = true; // release switch

    // CEM 3310 (Rev 3): attack overshoots to ~1.37
    static constexpr float cemAttackTarget = 1.3678794411714423f;
    // SSM 2050 (Rev 1/2): more linear, attack target closer to 1.0
    static constexpr float ssmAttackTarget = 1.05f;
};
