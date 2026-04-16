#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * CEM 3310 Envelope Generator
 *
 * Models the analog ADSR envelope of the Prophet-5.
 * Attack: exponential charge toward overshoot target (like RC circuit)
 * Decay/Release: exponential decay (natural capacitor discharge)
 *
 * The CEM 3310 charges toward ~1.37x the target (exp overshoot),
 * which gives the characteristic fast-start attack curve.
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
    float sustainLevel = 0.7f;

    float currentValue = 0.0f;
    Stage stage = Stage::Idle;

    // CEM 3310 attack overshoots to ~1.37 (1 / (1 - 1/e))
    // This creates the characteristic exponential attack curve
    static constexpr float attackTarget = 1.3678794411714423f;
};
