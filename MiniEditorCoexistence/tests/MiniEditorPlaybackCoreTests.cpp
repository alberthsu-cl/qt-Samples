#include "PlaybackCoreBoundary.h"

#include <iostream>

int main()
{
    using namespace mini_editor::playback_core;

    static_assert(CoreApiVersion::major == 1,
                  "The test documents the initial playback-core API level.");

    if (!hasExpectedCoreApiVersion()) {
        std::cerr << "Playback core API version is not supported.\n";
        return 1;
    }

    return 0;
}
