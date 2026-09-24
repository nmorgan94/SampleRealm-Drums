#include "Parameters.h"
#include "dsp/ClickGenerator.h"
#include "dsp/Drive.h"
#include "params/ParameterFormats.h"

//==============================================================================
namespace
{
    using Float = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    using namespace srd::params;

    std::unique_ptr<Float> gainParam (const juce::ParameterID& id, const juce::String& name,
                                      float min, float max, float defaultValue)
    {
        return std::make_unique<Float> (id, name, Range (min, max, 0.1f), defaultValue, decibels (Parameters::minusInfinityDb));
    }

    std::unique_ptr<Float> percentParam (const juce::ParameterID& id, const juce::String& name,
                                         float max, float defaultValue)
    {
        return std::make_unique<Float> (id, name, Range (0.0f, max, 0.1f), defaultValue, percent());
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global. Switching instrument swaps the whole UI, so it isn't automatable
    layout.add (std::make_unique<juce::AudioParameterChoice> (instrumentId, "Instrument", instrumentNames, 0,
                                                              juce::AudioParameterChoiceAttributes().withAutomatable (false)),
                gainParam (outputGainId, "Output", minusInfinityDb, 6.0f, -2.5f),
                std::make_unique<Float> (tuneId, "Tune", Range (-24.0f, 24.0f, 0.01f), 0.0f, withSuffix (" st", 2)),
                std::make_unique<juce::AudioParameterChoice> (keyTrackId, "Key Track", juce::StringArray { "Off", "On" }, 0),
                std::make_unique<Float> (lengthId, "Length", skewedRange (0.25f, 4.0f, 1.0f, 0.01f), 1.0f, withSuffix ("x", 2)),
                percentParam (pitchDepthId, "Pitch Depth", 200.0f, 100.0f));

    // Sub
    layout.add (gainParam (subLevelId, "Sub Level", minusInfinityDb, 6.0f, 0.0f),
                percentParam (subHarmonicsId, "Harmonics", 100.0f, 0.0f),
                std::make_unique<Float> (subPhaseId, "Phase", Range (0.0f, 90.0f, 1.0f), 0.0f,
                                         withSuffix (juce::String::fromUTF8 ("\xc2\xb0"), 0)));

    // Click
    layout.add (std::make_unique<juce::AudioParameterChoice> (clickTypeId, "Click Type", srd::ClickGenerator::getTypeNames(), 0),
                gainParam (clickLevelId, "Click Level", minusInfinityDb, 6.0f, -12.0f),
                std::make_unique<Float> (clickToneId, "Click Tone", skewedRange (200.0f, 16000.0f, 3000.0f), 4000.0f, hertz()),
                std::make_unique<Float> (clickDecayId, "Click Decay", skewedRange (1.0f, 100.0f, 15.0f, 0.1f), 12.0f, withSuffix (" ms", 1)),
                std::make_unique<Float> (clickPitchId, "Click Pitch", skewedRange (200.0f, 10000.0f, 2000.0f), 2500.0f, hertz()));

    // Snare body
    layout.add (gainParam (snareBodyLevelId, "Body Level", minusInfinityDb, 6.0f, -4.0f),
                percentParam (snareBodyHarmonicsId, "Body Harmonics", 100.0f, 20.0f));

    // Snare noise
    layout.add (gainParam (snareNoiseLevelId, "Noise Level", minusInfinityDb, 6.0f, 0.0f),
                std::make_unique<Float> (snareNoiseLowCutId, "Noise Low Cut", skewedRange (100.0f, 8000.0f, 1500.0f), 800.0f, hertz()),
                std::make_unique<Float> (snareNoiseHighCutId, "Noise High Cut", skewedRange (1000.0f, 18000.0f, 6000.0f), 12000.0f, hertz()));

    // Snare snap
    layout.add (std::make_unique<juce::AudioParameterChoice> (snareSnapTypeId, "Snap Type", srd::ClickGenerator::getTypeNames(), 0),
                gainParam (snareSnapLevelId, "Snap Level", minusInfinityDb, 6.0f, -6.0f),
                std::make_unique<Float> (snareSnapToneId, "Snap Tone", skewedRange (200.0f, 16000.0f, 3000.0f), 3500.0f, hertz()),
                std::make_unique<Float> (snareSnapDecayId, "Snap Decay", skewedRange (1.0f, 100.0f, 15.0f, 0.1f), 8.0f, withSuffix (" ms", 1)),
                std::make_unique<Float> (snareSnapPitchId, "Snap Pitch", skewedRange (200.0f, 10000.0f, 2000.0f), 2500.0f, hertz()));

    // Drive
    layout.add (std::make_unique<juce::AudioParameterChoice> (driveTypeId, "Drive Type", srd::Drive::getTypeNames(), 0),
                percentParam (driveAmountId, "Drive", 100.0f, 20.0f),
                percentParam (driveMixId, "Drive Mix", 100.0f, 100.0f));

    // EQ
    layout.add (gainParam (eqLowGainId, "Low", -12.0f, 12.0f, 0.0f),
                std::make_unique<Float> (eqMidFreqId, "Mid Freq", skewedRange (100.0f, 5000.0f, 600.0f), 400.0f, hertz()),
                gainParam (eqMidGainId, "Mid", -12.0f, 12.0f, 0.0f),
                gainParam (eqHighGainId, "High", -12.0f, 12.0f, 0.0f));

    // Clip
    layout.add (percentParam (clipAmountId, "Clip", 100.0f, 0.0f));

    return layout;
}
