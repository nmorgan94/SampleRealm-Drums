#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include "BuildType.h"
#include "DrumEnvelopes.h"
#include "ParameterSpecs.h"

//==============================================================================
inline std::string uiInfoJson (const char* version, const char* commit)
{
    const auto quoted = [] (const char* text)
    {
        std::string result = "\"";

        for (auto* c = text; *c != '\0'; ++c)
        {
            if (*c == '"' || *c == '\\')
                result += '\\';

            result += *c;
        }

        return result + "\"";
    };

    // Not printf, which follows the host's locale. Seven significant digits, as a float holds
    const auto number = [] (float value)
    {
        std::string::size_type decimals = 6;
        long long scale = 1000000;
        double factor = 1000000.0;

        for (auto limit = 10.0f; std::abs (value) >= limit && decimals > 0; limit *= 10.0f)
        {
            --decimals;
            scale /= 10;
            factor /= 10.0;
        }

        const auto scaled = std::llround (value * factor);
        const auto magnitude = std::llabs (scaled);
        auto text = std::string (scaled < 0 ? "-" : "") + std::to_string (magnitude / scale);

        if (const auto fraction = magnitude % scale; fraction != 0)
        {
            auto digits = std::to_string (fraction);
            digits.insert (0, decimals - digits.size(), '0');
            digits.erase (digits.find_last_not_of ('0') + 1);
            text += "." + digits;
        }

        return text;
    };

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

    std::string json = "{\"version\":" + quoted (version) + ",\"commit\":" + quoted (commit)
                     + ",\"build\":" + quoted (BuildType::name)
                     + ",\"minusInfinityDb\":" + number (ParameterSpecs::minusInfinityDb) + ",\"params\":[";

    for (size_t i = 0; i < ParameterSpecs::all.size(); ++i)
    {
        const auto& spec = *ParameterSpecs::all[i];

        std::string choices;
        for (auto* choice : spec.choices)
            choices += (choices.empty() ? "" : ",") + quoted (choice);

        json += (i == 0 ? "{" : ",{");
        json += "\"id\":" + quoted (spec.id) + ",\"name\":" + quoted (spec.name);
        json += ",\"kind\":" + quoted (spec.kind == ParameterSpecs::Kind::choice ? "choice" : "real");
        json += ",\"min\":" + number (spec.min) + ",\"max\":" + number (spec.max);
        json += ",\"defaultValue\":" + number (spec.defaultValue);
        json += ",\"step\":" + number (spec.step) + ",\"skew\":" + number (ParameterSpecs::rangeOf (spec).skew);
        json += ",\"format\":" + quoted (formatName (spec.format)) + ",\"suffix\":" + quoted (spec.suffix);
        json += ",\"decimals\":" + std::to_string (spec.decimals);
        json += ",\"automatable\":" + std::string (spec.automatable ? "true" : "false");
        json += ",\"choices\":[" + choices + "]}";
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

    return json + "]}";
}
