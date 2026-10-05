#pragma once

#include "windows_live_ime/core/model.hpp"

#include <functional>

namespace windows_live_ime::core {

class ConversionEngine {
public:
    using Completion = std::function<void(ConversionResponse)>;

    virtual ~ConversionEngine() = default;

    // Implementations must enqueue work and return promptly. Completion may run
    // on a worker thread; callers marshal accepted results to their UI thread.
    virtual void submit(ConversionRequest request, Completion completion) = 0;
};

}  // namespace windows_live_ime::core
