#pragma once

#include <cstdint>

// This header marks the public boundary of the future playback engine. It is
// intentionally small during Milestone 1: the important result is that a
// separate C++17 target exists before engine behavior is moved into it.
//
// Public headers in this directory may use only standard C++ headers. Qt,
// MFC, Win32, codecs, and graphics APIs belong in adapters outside this core.
namespace mini_editor::playback_core {

struct CoreApiVersion final {
    static constexpr std::uint32_t major = 1;
    static constexpr std::uint32_t minor = 0;
};

constexpr bool hasExpectedCoreApiVersion()
{
    return CoreApiVersion::major == 1;
}

} // namespace mini_editor::playback_core
