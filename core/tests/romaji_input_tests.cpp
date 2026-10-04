#include "windows_live_ime/core/romaji_input.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
}

int main() {
    using namespace windows_live_ime::core;
    try {
        const struct { std::u32string_view raw; std::u32string_view kana; } examples[] = {
            {U"nihongo", U"にほんご"}, {U"konnichiwa", U"こんにちわ"},
            {U"kyouhatennkigaii", U"きょうはてんきがいい"}, {U"gakkou", U"がっこう"},
            {U"matcha", U"まっちゃ"}, {U"shin'you", U"しんよう"}, {U"shinyou", U"しにょう"},
            {U"NN", U"ん"}, {U"nna", U"んな"}, {U"xtsuxya", U"っゃ"},
            {U"thi-fa,", U"てぃーふぁ、"}, {U"[nihongo].", U"「にほんご」。"},
            {U"かな😀", U"かな😀"}, {U"123", U"123"}
        };
        for (const auto& example : examples) {
            require(roman_to_kana(example.raw).text() == example.kana, "romaji example failed");
        }
        RomajiInput input;
        input.insert(U"nihon");
        require(input.reading().kana == U"にほ" && input.reading().pending == U"n", "trailing n must remain editable");
        require(input.reading(true).text() == U"にほん", "commit must finalize trailing n");
        input.insert(U"go");
        require(input.reading().text() == U"にほんご", "incremental typing must combine pending n");
        require(input.erase_previous(), "backspace should remove a key");
        require(input.reading().text() == U"にほんg", "backspace must undo the last syllable key");
        input.insert(U"a");
        require(input.reading().text() == U"にほんが", "correction must reparse the syllable");
        input.clear();
        input.insert(U"kaki");
        input.move_left(); input.move_left();
        input.insert(U"na");
        require(input.reading().text() == U"かなき", "inserting in the middle must preserve the suffix");
        require(input.display_cursor() == 2, "display cursor must use kana scalar positions");
        require(input.erase_next(), "delete should remove a following key");
        require(input.raw() == U"kanai", "delete must use the raw cursor");
        input.clear();
        require(!input.erase_previous() && !input.erase_next(), "empty edits must be harmless");
        require(to_katakana(U"にほんご、がぱゔ。") == U"ニホンゴ、ガパヴ。", "katakana conversion failed");
        require(to_katakana(U"にほんご、がぱゔ。", true) == U"ﾆﾎﾝｺﾞ､ｶﾞﾊﾟｳﾞ｡", "half-width voiced kana conversion failed");
        require(to_full_width_ascii(U"Live 123!") == U"Ｌｉｖｅ　１２３！", "full-width ASCII conversion failed");
        std::cout << "Romaji input tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Romaji input failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
