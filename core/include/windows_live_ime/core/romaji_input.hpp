#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace windows_live_ime::core {

struct KanaReading {
    std::u32string kana;
    std::u32string pending;

    [[nodiscard]] std::u32string text() const { return kana + pending; }
};

[[nodiscard]] KanaReading roman_to_kana(std::u32string_view input, bool finalize = false);
[[nodiscard]] std::u32string to_katakana(std::u32string_view text, bool half_width = false);
[[nodiscard]] std::u32string to_full_width_ascii(std::u32string_view text);

// Cursor positions refer to raw input so Backspace can undo the last key,
// including keys which formed a digraph, a doubled consonant, or an n syllable.
class RomajiInput final {
public:
    void insert(std::u32string_view text);
    [[nodiscard]] bool erase_previous();
    [[nodiscard]] bool erase_next();
    void move_left() noexcept;
    void move_right() noexcept;
    void clear() noexcept;
    [[nodiscard]] KanaReading reading(bool finalize = false) const;
    [[nodiscard]] const std::u32string& raw() const noexcept { return raw_; }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t display_cursor() const;
    [[nodiscard]] bool empty() const noexcept { return raw_.empty(); }

private:
    std::u32string raw_;
    std::size_t cursor_{0};
};

}  // namespace windows_live_ime::core
