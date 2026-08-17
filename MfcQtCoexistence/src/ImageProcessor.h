#pragma once

#include "EffectSettings.h"

#include <afxwin.h>
#include <atlimage.h>

// Image processing is deliberately separate from MainFrame. In Phase 2, the
// Qt dialog can still select EffectType while this implementation remains the
// same, or it can later be replaced by a Qt/QImage implementation.
namespace ImageProcessor {

bool createDefaultImage(CImage &image);
bool loadImage(const CString &fileName, CImage &image);
bool applyEffect(const CImage &source, EffectType effect, CImage &result);

} // namespace ImageProcessor
