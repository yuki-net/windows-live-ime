#pragma once

#include "windows_live_ime/core/model.hpp"

#include <atomic>

namespace windows_live_ime::core {

class LiveConversionCoordinator final {
public:
    [[nodiscard]] ConversionRequest make_request(std::u32string input);
    [[nodiscard]] GenerationId invalidate() noexcept;
    [[nodiscard]] bool is_current(const ConversionResponse& response) const noexcept;
    [[nodiscard]] GenerationId current_generation() const noexcept;

private:
    std::atomic<RequestId> next_request_id_{0};
    std::atomic<GenerationId> next_generation_id_{0};
    std::atomic<GenerationId> current_generation_id_{0};
};

}  // namespace windows_live_ime::core
