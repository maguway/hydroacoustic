#pragma once
#include "wav_loader.h"
#include <ostream>

namespace sd
{
    void writeScore(std::ostream& os, const WavData& wav);
}
