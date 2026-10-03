#include "windows_live_ime/core/live_conversion_coordinator.hpp"

namespace windows_live_ime::core {

ConversionRequest LiveConversionCoordinator::make_request(std::u32string input) {
    const auto request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto generation_id = next_generation_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    current_generation_id_.store(generation_id, std::memory_order_release);

    return ConversionRequest{
        .request_id = request_id,
        .generation_id = generation_id,
        .input = std::move(input),
    };
}

GenerationId LiveConversionCoordinator::invalidate() noexcept {
    const auto generation_id = next_generation_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    current_generation_id_.store(generation_id, std::memory_order_release);
    return generation_id;
}

bool LiveConversionCoordinator::is_current(const ConversionResponse& response) const noexcept {
    return response.generation_id == current_generation_id_.load(std::memory_order_acquire);
}

GenerationId LiveConversionCoordinator::current_generation() const noexcept {
    return current_generation_id_.load(std::memory_order_acquire);
}

}  // namespace windows_live_ime::core
