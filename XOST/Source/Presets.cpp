#include "Presets.h"
#include "BinaryData.h"

namespace
{
    std::vector<Presets::Entry> parse (const char* data, int size)
    {
        std::vector<Presets::Entry> out;
        juce::StringArray lines;
        lines.addLines (juce::String::fromUTF8 (data, size));
        out.reserve ((size_t) lines.size());
        for (const auto& line : lines)
        {
            if (line.trim().isEmpty() || line.startsWithChar ('#')) continue;
            auto f = juce::StringArray::fromTokens (line, "\t", "");
            if (f.size() < 5) continue;
            Presets::Entry e;
            e.name = f[0]; e.category = f[1]; e.description = f[3]; e.values = f[4];
            e.tags.addTokens (f[2], " ", "");
            e.tags.removeEmptyStrings();
            if (f.size() > 5) e.kind = f[5];
            out.push_back (std::move (e));
        }
        return out;
    }
}

const std::vector<Presets::Entry>& Presets::factory()
{
    static const auto v = parse (BinaryData::FactoryPresets_txt, BinaryData::FactoryPresets_txtSize);
    return v;
}
const std::vector<Presets::Entry>& Presets::matrixBank()
{
    static const auto v = parse (BinaryData::MatrixPresets_txt, BinaryData::MatrixPresets_txtSize);
    return v;
}
const std::vector<Presets::Entry>& Presets::rackBank()
{
    static const auto v = parse (BinaryData::RackPresets_txt, BinaryData::RackPresets_txtSize);
    return v;
}

const std::vector<Presets::Entry>& Presets::seeds()
{
    static const auto v = parse (BinaryData::SeedPatches_txt, BinaryData::SeedPatches_txtSize);
    return v;
}

int Presets::count() { return (int) factory().size(); }
static bool valid (int i) { return juce::isPositiveAndBelow (i, Presets::count()); }
juce::String Presets::name (int i)        { return valid (i) ? factory()[(size_t) i].name : juce::String(); }
juce::String Presets::category (int i)    { return valid (i) ? factory()[(size_t) i].category : juce::String(); }
juce::String Presets::description (int i) { return valid (i) ? factory()[(size_t) i].description : juce::String(); }
juce::StringArray Presets::tags (int i)   { return valid (i) ? factory()[(size_t) i].tags : juce::StringArray(); }
