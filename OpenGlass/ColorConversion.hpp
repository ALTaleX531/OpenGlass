#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"

namespace OpenGlass::ColorConversion
{
    // Helper classes used for converting between RGB, HSV, and hex.
    class Rgb
    {
    public:
        double r{};
        double g{};
        double b{};
        Rgb() = default;
        Rgb(double r, double g, double b);
    };

    class Hsv
    {
    public:
        double h{};
        double s{};
        double v{};
        Hsv() = default;
        Hsv(double h, double s, double v);
    };

    std::optional<unsigned long> TryParseInt(const std::wstring_view& s);
    std::optional<unsigned long> TryParseInt(const std::wstring_view& str, int base);

    Hsv RgbToHsv(const Rgb& rgb);
    Rgb HsvToRgb(const Hsv& hsv);

    Rgb HexToRgb(const std::wstring_view& input);
    winrt::hstring RgbToHex(const Rgb& rgb);

    std::tuple<Rgb, double> HexToRgba(const std::wstring_view& input);
    winrt::hstring RgbaToHex(const Rgb& rgb, double alpha);

    wu::Color ColorFromRgba(const Rgb& rgb, double alpha = 1.0);
    Rgb RgbFromColor(const wu::Color& color);

    // We represent HSV and alpha using a Vector4 (float4 in C++/WinRT).
    // We'll use the following helper methods to convert between the four dimensions and HSVA.
    namespace hsv
    {
        inline float GetHue(const wfn::float4& hsva) { return hsva.x; }
        inline void SetHue(wfn::float4& hsva, float hue) { hsva.x = hue; }
        inline float GetSaturation(const wfn::float4& hsva) { return hsva.y; }
        inline void SetSaturation(wfn::float4& hsva, float saturation) { hsva.y = saturation; }
        inline float GetValue(const wfn::float4& hsva) { return hsva.z; }
        inline void SetValue(wfn::float4& hsva, float value) { hsva.z = value; }
        inline float GetAlpha(const wfn::float4& hsva) { return hsva.w; }
        inline void SetAlpha(wfn::float4& hsva, float alpha) { hsva.w = alpha; }
    }

}