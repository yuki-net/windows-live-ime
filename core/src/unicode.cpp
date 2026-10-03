#include "windows_live_ime/core/unicode.hpp"

#include <stdexcept>

namespace windows_live_ime::core {
namespace {

constexpr char32_t kReplacementThreshold = 0x80;
constexpr char32_t kMaximumUnicodeScalar = 0x10FFFF;

bool is_continuation(unsigned char byte) noexcept {
    return (byte & 0xC0) == 0x80;
}

}  // namespace

std::string encode_utf8(std::u32string_view text) {
    std::string encoded;
    encoded.reserve(text.size());

    for (const char32_t scalar : text) {
        if (scalar > kMaximumUnicodeScalar || (scalar >= 0xD800 && scalar <= 0xDFFF)) {
            throw std::invalid_argument("Text contains an invalid Unicode scalar value.");
        }

        if (scalar < kReplacementThreshold) {
            encoded.push_back(static_cast<char>(scalar));
        } else if (scalar < 0x800) {
            encoded.push_back(static_cast<char>(0xC0 | (scalar >> 6)));
            encoded.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
        } else if (scalar < 0x10000) {
            encoded.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
            encoded.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
            encoded.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
        } else {
            encoded.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
            encoded.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
            encoded.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
            encoded.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
        }
    }

    return encoded;
}

std::optional<std::u32string> decode_utf8(std::string_view text) noexcept {
    std::u32string decoded;
    decoded.reserve(text.size());

    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        char32_t scalar = 0;
        std::size_t continuation_count = 0;
        char32_t minimum_scalar = 0;

        if (first < 0x80) {
            scalar = first;
        } else if ((first & 0xE0) == 0xC0) {
            scalar = first & 0x1F;
            continuation_count = 1;
            minimum_scalar = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            scalar = first & 0x0F;
            continuation_count = 2;
            minimum_scalar = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            scalar = first & 0x07;
            continuation_count = 3;
            minimum_scalar = 0x10000;
        } else {
            return std::nullopt;
        }

        if (index + continuation_count >= text.size()) {
            return std::nullopt;
        }

        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto byte = static_cast<unsigned char>(text[index + offset]);
            if (!is_continuation(byte)) {
                return std::nullopt;
            }
            scalar = (scalar << 6) | (byte & 0x3F);
        }

        if ((continuation_count != 0 && scalar < minimum_scalar) ||
            scalar > kMaximumUnicodeScalar || (scalar >= 0xD800 && scalar <= 0xDFFF)) {
            return std::nullopt;
        }

        decoded.push_back(scalar);
        index += continuation_count + 1;
    }

    return decoded;
}

}  // namespace windows_live_ime::core
