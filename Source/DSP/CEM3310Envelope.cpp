#include "CEM3310Envelope.h"

void CEM3310Envelope::prepare(double sr)
{
    sampleRate = sr;
    currentValue = 0.0f;
    stage = Stage::Idle;
}

void CEM3310Envelope::setAttack(float seconds)
{
    attackCoeff = calcCoeff(juce::jlimit(0.001f, 10.0f, seconds));
}

void CEM3310Envelope::setDecay(float seconds)
{
    decayCoeff = calcCoeff(juce::jlimit(0.001f, 10.0f, seconds));
}

void CEM3310Envelope::setSustain(float level)
{
    sustainLevel = juce::jlimit(0.0f, 1.0f, level);
}

void CEM3310Envelope::setRelease(float seconds)
{
    releaseCoeff = calcCoeff(juce::jlimit(0.001f, 10.0f, seconds));
}

float CEM3310Envelope::calcCoeff(float timeSeconds) const
{
    // One-pole coefficient for exponential envelope
    // Reaches ~63% in the given time (RC time constant)
    if (timeSeconds <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * timeSeconds));
}

void CEM3310Envelope::noteOn()
{
    stage = Stage::Attack;
}

void CEM3310Envelope::noteOff()
{
    if (stage != Stage::Idle)
        stage = Stage::Release;
}

float CEM3310Envelope::process()
{
    switch (stage)
    {
        case Stage::Idle:
            currentValue = 0.0f;
            break;

        case Stage::Attack:
            // Charge toward overshoot target (1.37), mimicking CEM 3310
            // RC charge: the capacitor charges toward a voltage higher
            // than the comparator threshold, giving a fast exponential rise
            currentValue += attackCoeff * (attackTarget - currentValue);

            if (currentValue >= 1.0f)
            {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;

        case Stage::Decay:
            // Exponential decay toward sustain level
            currentValue += decayCoeff * (sustainLevel - currentValue);

            // Transition to sustain when close enough
            if (std::abs(currentValue - sustainLevel) < 1e-5f)
            {
                currentValue = sustainLevel;
                stage = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            currentValue = sustainLevel;
            break;

        case Stage::Release:
            // Exponential decay toward zero
            currentValue += releaseCoeff * (0.0f - currentValue);

            if (currentValue < 1e-5f)
            {
                currentValue = 0.0f;
                stage = Stage::Idle;
            }
            break;
    }

    return currentValue;
}
