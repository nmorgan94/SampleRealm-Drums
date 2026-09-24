#include "ClickGenerator.h"

namespace srd
{
    void ClickGenerator::prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        filter.prepare ({ newSampleRate, 1, 1 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.setResonance (0.9f);
        sweep.prepare (newSampleRate);

        active = false;
    }

    void ClickGenerator::trigger (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        const auto nyquistLimit = 0.45f * sampleRate;
        settings.toneHz  = juce::jlimit (20.0f, nyquistLimit, settings.toneHz);
        settings.pitchHz = juce::jlimit (20.0f, nyquistLimit, settings.pitchHz);

        const auto decaySamples = juce::jmax (1.0f, settings.decayMs * 0.001f * sampleRate);
        decayCoefficient = std::exp (std::log (0.001f) / decaySamples);

        sampleIndex = 0;
        envelope = 1.0f;
        sweep.reset (0.0f);
        sweepHz = settings.pitchHz;
        sweepCoefficient = std::exp (std::log (0.125f) / decaySamples); // three octaves over the decay
        impulseHalfPeriod = juce::jmax (1, juce::roundToInt (sampleRate / (2.0f * settings.toneHz)));

        filter.reset();
        filter.setCutoffFrequency (settings.toneHz);
        random.setSeed (1); // the same noise every hit

        active = settings.gain > 0.0f;
    }

    float ClickGenerator::next() noexcept
    {
        if (! active)
            return 0.0f;

        const auto ramping = settings.type != Type::impulse && sampleIndex < attackSamples;
        const auto attack  = ramping ? (float) (sampleIndex + 1) / (float) attackSamples : 1.0f;
        float source = 0.0f;

        switch (settings.type)
        {
            case Type::noise:
                source = 2.0f * filter.processSample (0, random.nextFloat() * 2.0f - 1.0f);
                break;

            case Type::sweep:
                source = sweep.next (sweepHz);
                sweepHz *= sweepCoefficient;
                break;

            case Type::impulse:
                if (sampleIndex < 2 * impulseHalfPeriod)
                    source = sampleIndex < impulseHalfPeriod ? 1.0f : -1.0f;
                break;
        }

        const auto out = source * attack * envelope * settings.gain;

        ++sampleIndex;
        envelope *= decayCoefficient;

        if (envelope < silence)
            active = false;

        return out;
    }
}
