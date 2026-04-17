#include "CEM3310Envelope.h"

void CEM3310Envelope::prepare(double sr)
{
    sampleRate = sr;
    currentValue = 0.0f;
    stage = Stage::Idle;
    // Pre-compute fast release for when release switch is OFF
    fastReleaseCoeff = calcCoeff(0.005f);  // 5ms fast release
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
    // Choose envelope curve based on Rev mode
    bool linear = (revMode == 0);  // Rev 1/2 = nearly linear

    switch (stage)
    {
        case Stage::Idle:
            currentValue = 0.0f;
            break;

        case Stage::Attack:
        {
            if (linear)
            {
                // SSM 2050 (Rev 1/2): nearly linear attack
                // Charge toward slightly above 1.0 for a gentle curve
                currentValue += attackCoeff * (ssmAttackTarget - currentValue);
            }
            else
            {
                // CEM 3310 (Rev 3): exponential attack with overshoot
                currentValue += attackCoeff * (cemAttackTarget - currentValue);
            }

            if (currentValue >= 1.0f)
            {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;
        }

        case Stage::Decay:
        {
            if (linear)
            {
                // SSM 2050: more linear decay
                // Use a blend: mostly linear with slight curve
                float linearStep = decayCoeff * (1.0f - sustainLevel) * 0.5f;
                float expStep = decayCoeff * (sustainLevel - currentValue);
                currentValue += linearStep * 0.3f + expStep * 0.7f;
            }
            else
            {
                // CEM 3310: exponential decay
                currentValue += decayCoeff * (sustainLevel - currentValue);
            }

            if (std::abs(currentValue - sustainLevel) < 1e-5f)
            {
                currentValue = sustainLevel;
                stage = Stage::Sustain;
            }
            break;
        }

        case Stage::Sustain:
            currentValue = sustainLevel;
            break;

        case Stage::Release:
        {
            // Use fast release if release switch is OFF
            float relCoeff = releaseEnabled ? releaseCoeff : fastReleaseCoeff;

            if (linear)
            {
                // SSM 2050: more linear release
                float linearStep = relCoeff * currentValue * 0.5f;
                float expStep = relCoeff * (0.0f - currentValue);
                currentValue += linearStep * 0.3f + expStep * 0.7f;
            }
            else
            {
                // CEM 3310: exponential release
                currentValue += relCoeff * (0.0f - currentValue);
            }

            if (currentValue < 1e-5f)
            {
                currentValue = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }
    }

    return currentValue;
}
