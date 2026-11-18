#pragma once

#include <string>
#include <samplerealm/UiInfoJson.h>
#include "DrumEnvelopes.h"
#include "ParameterSpecs.h"
#include "Patterns.h"

//==============================================================================
/** What the web editor needs beside the shared ui info: how each value reads, in the host's order, the envelopes
    and the loop's patterns.
    As DrumInfo in webui/src/backend.ts. */
inline std::string drumInfoJson()
{
    using JsonText::number, JsonText::quoted;

    const auto formatName = [] (ParameterSpecs::Format format)
    {
        switch (format)
        {
            case ParameterSpecs::Format::decibels: return "decibels";
            case ParameterSpecs::Format::hertz:    return "hertz";
            case ParameterSpecs::Format::suffix:   break;
        }

        return "suffix";
    };

    std::string json = "{\"minusInfinityDb\":" + number (ParameterSpecs::minusInfinityDb) + ",\"text\":[";

    for (size_t i = 0; i < ParameterSpecs::drumSpecs.size(); ++i)
    {
        const auto& spec = *ParameterSpecs::drumSpecs[i];

        json += (i == 0 ? "{" : ",{");
        json += "\"format\":" + quoted (formatName (spec.format)) + ",\"suffix\":" + quoted (spec.suffix);
        json += ",\"decimals\":" + std::to_string (spec.decimals) + "}";
    }

    json += "],\"envelopes\":[";

    const auto specs = DrumEnvelopes::createSpecs();

    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& spec = specs[i];

        std::string nodes;
        for (const auto& node : spec.defaultNodes)
            nodes += (nodes.empty() ? "{" : ",{") + std::string ("\"time\":") + number (node.time)
                   + ",\"value\":" + number (node.value) + ",\"curve\":" + number (node.curve) + "}";

        json += (i == 0 ? "{" : ",{");
        json += "\"id\":" + quoted (spec.id);
        json += ",\"scale\":" + quoted (spec.scale == srd::EnvelopeSpec::Scale::logarithmic ? "logarithmic" : "linear");
        json += ",\"min\":" + number (spec.minValue) + ",\"max\":" + number (spec.maxValue);
        json += ",\"maxTime\":" + number (spec.maxTime) + ",\"nodes\":[" + nodes + "]}";
    }

    json += "],\"patterns\":[";

    for (size_t i = 0; i < Patterns::names.size(); ++i)
        json += (i == 0 ? "" : ",") + quoted (Patterns::names[i]);

    return json + "]}";
}
