#pragma once

#include <QMetaType>

// A small, strongly typed message sent from the UI thread to the worker.
enum class EffectType {
    Grayscale,
    Invert,
    Blur
};

Q_DECLARE_METATYPE(EffectType)
