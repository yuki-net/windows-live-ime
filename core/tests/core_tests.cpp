#include "windows_live_ime/core/conversion_engine.hpp"
#include "windows_live_ime/core/live_conversion_coordinator.hpp"
#include "windows_live_ime/core/unicode.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_live_conversion_generations() {
    using namespace windows_live_ime::core;

    LiveConversionCoordinator coordinator;
    const auto first = coordinator.make_request(U"かな");
    const auto second = coordinator.make_request(U"かな漢字");

    require(first.request_id == 1, "first request id should be one");
    require(second.request_id == 2, "request ids should increase");
    require(second.generation_id > first.generation_id, "generation ids should increase");
    require(!coordinator.is_current(ConversionResponse{first.request_id, first.generation_id, {}}),
            "older results must be rejected");
    require(coordinator.is_current(ConversionResponse{second.request_id, second.generation_id, {}}),
            "the newest generation must be accepted");

    const auto invalidated_generation = coordinator.invalidate();
    require(invalidated_generation > second.generation_id, "invalidate must advance generation");
    require(!coordinator.is_current(ConversionResponse{second.request_id, second.generation_id, {}}),
            "responses must be rejected after invalidation");
}

void test_utf8_round_trip() {
    using namespace windows_live_ime::core;
    constexpr auto expected = U"かな漢字😀";
    const auto encoded = encode_utf8(expected);
    const auto decoded = decode_utf8(encoded);

    require(decoded.has_value(), "valid UTF-8 should decode");
    require(*decoded == expected, "UTF-8 round trip should preserve Japanese text and supplementary scalars");
    require(!decode_utf8(std::string("\xC0\xAF", 2)).has_value(), "overlong UTF-8 must be rejected");
    require(!decode_utf8(std::string("\xED\xA0\x80", 3)).has_value(), "UTF-8 surrogate values must be rejected");
}

}  // namespace

int main() {
    try {
        test_live_conversion_generations();
        test_utf8_round_trip();
        std::cout << "Core tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Core test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
