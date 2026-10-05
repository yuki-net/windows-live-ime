#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace windows_live_ime::core {

[[nodiscard]] std::string encode_utf8(std::u32string_view text);
[[nodiscard]] std::optional<std::u32string> decode_utf8(std::string_view text) noexcept;

}  // namespace windows_live_ime::core
