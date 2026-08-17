#include "ImageProcessor.h"

#include <algorithm>

namespace {

COLORREF grayscale(COLORREF color)
{
    const int value = (30 * GetRValue(color) + 59 * GetGValue(color)
                       + 11 * GetBValue(color)) / 100;
    return RGB(value, value, value);
}

COLORREF invert(COLORREF color)
{
    return RGB(255 - GetRValue(color),
               255 - GetGValue(color),
               255 - GetBValue(color));
}

COLORREF blurAt(const CImage &source, int x, int y)
{
    int red = 0;
    int green = 0;
    int blue = 0;

    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        const int sampleY = std::clamp(y + offsetY, 0, source.GetHeight() - 1);
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const int sampleX = std::clamp(x + offsetX, 0, source.GetWidth() - 1);
            const COLORREF color = source.GetPixel(sampleX, sampleY);
            red += GetRValue(color);
            green += GetGValue(color);
            blue += GetBValue(color);
        }
    }

    return RGB(red / 9, green / 9, blue / 9);
}

} // namespace

bool ImageProcessor::createDefaultImage(CImage &image)
{
    image.Destroy();

    constexpr int width = 960;
    constexpr int height = 540;
    if (FAILED(image.Create(width, height, 32)))
        return false;

    // A generated image means the learning sample works without any external
    // files. The color blocks make grayscale, inversion, and blur visible.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int red = x * 255 / (width - 1);
            const int green = y * 255 / (height - 1);
            const int blue = 130 + ((x / 80 + y / 80) % 2) * 100;
            image.SetPixel(x, y, RGB(red, green, blue));
        }
    }

    return true;
}

bool ImageProcessor::loadImage(const CString &fileName, CImage &image)
{
    image.Destroy();
    return SUCCEEDED(image.Load(fileName));
}

bool ImageProcessor::applyEffect(const CImage &source,
                                 EffectType effect,
                                 CImage &result)
{
    if (source.IsNull())
        return false;

    result.Destroy();
    if (FAILED(result.Create(source.GetWidth(), source.GetHeight(), 32)))
        return false;

    for (int y = 0; y < source.GetHeight(); ++y) {
        for (int x = 0; x < source.GetWidth(); ++x) {
            const COLORREF sourceColor = source.GetPixel(x, y);
            COLORREF resultColor = sourceColor;

            switch (effect) {
            case EffectType::None:
                break;
            case EffectType::Grayscale:
                resultColor = grayscale(sourceColor);
                break;
            case EffectType::Invert:
                resultColor = invert(sourceColor);
                break;
            case EffectType::Blur:
                resultColor = blurAt(source, x, y);
                break;
            }

            result.SetPixel(x, y, resultColor);
        }
    }

    return true;
}
