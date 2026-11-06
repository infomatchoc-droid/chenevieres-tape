// Chènevières Tape — polices, couleurs et images communes (mêmes codes que Comp et EQ)
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

namespace look
{
inline const juce::Colour gold { 0xffe8a33a };
inline const juce::Colour goldDim { 0xffc8892f };
inline const juce::Colour text { 0xfff5ecde };
inline const juce::Colour textDim { 0xffaaa092 };
inline const juce::Colour label { 0xff968c80 };
inline const juce::Colour boxFill { 0xff161412 };
inline const juce::Colour boxLine { 0xff4e463c };

inline juce::Typeface::Ptr typeface (int weight)
{
    static juce::Typeface::Ptr t500 = juce::Typeface::createSystemTypefaceFor (BinaryData::RobotoCondensed500_ttf, BinaryData::RobotoCondensed500_ttfSize);
    static juce::Typeface::Ptr t600 = juce::Typeface::createSystemTypefaceFor (BinaryData::RobotoCondensed600_ttf, BinaryData::RobotoCondensed600_ttfSize);
    static juce::Typeface::Ptr t700 = juce::Typeface::createSystemTypefaceFor (BinaryData::RobotoCondensed700_ttf, BinaryData::RobotoCondensed700_ttfSize);
    return weight >= 700 ? t700 : weight >= 600 ? t600 : t500;
}

inline juce::Font font (float height, int weight = 700)
{
    return juce::Font (juce::FontOptions (typeface (weight)).withHeight (height));
}

inline juce::Image image (const char* data, int size)
{
    return juce::ImageCache::getFromMemory (data, size);
}
} // namespace look
