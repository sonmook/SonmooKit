#pragma once

namespace SonmooBrand
{
inline void draw(juce::Graphics& g, float x, float y, float height)
{
    constexpr auto mint = 0xff55e2cf;
    constexpr auto white = 0xfff4f7ff;
    const auto scale = height / 56.0f;
    juce::Path signal;
    signal.startNewSubPath(44, 17);
    signal.lineTo(28, 17);
    signal.cubicTo(20, 17, 17, 22, 17, 26);
    signal.cubicTo(17, 30, 20, 34, 28, 34);
    signal.lineTo(36, 34);
    signal.cubicTo(44, 34, 47, 39, 47, 43);
    signal.cubicTo(47, 47, 44, 51, 36, 51);
    signal.lineTo(20, 51);

    {
        juce::Graphics::ScopedSaveState restore(g);
        g.addTransform(juce::AffineTransform::scale(scale).translated(x - 9 * scale, y - 6 * scale));
        g.setColour(juce::Colour(mint));
        g.strokePath(signal, juce::PathStrokeType(5, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(42.5f, 12.5f, 9, 9);
        g.fillEllipse(12.5f, 46.5f, 9, 9);
    }

    juce::AttributedString wordmark;
    wordmark.setJustification(juce::Justification::centredLeft);
    const juce::Font font{juce::FontOptions("Segoe UI", height * .68f, juce::Font::bold)};
    wordmark.append("Sonmoo", font, juce::Colour(white));
    wordmark.append("Kit", font, juce::Colour(mint));
    wordmark.draw(g, {x + height * .94f, y, height * 6, height});
}
}
