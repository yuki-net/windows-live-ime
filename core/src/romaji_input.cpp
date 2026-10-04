#include "windows_live_ime/core/romaji_input.hpp"

#include <algorithm>
#include <array>

namespace windows_live_ime::core {
namespace {

struct Syllable { std::u32string_view roman; std::u32string_view kana; };
constexpr auto syllables = std::to_array<Syllable>({
    {U"a", U"あ"}, {U"i", U"い"}, {U"u", U"う"}, {U"e", U"え"}, {U"o", U"お"},
    {U"ka", U"か"}, {U"ki", U"き"}, {U"ku", U"く"}, {U"ke", U"け"}, {U"ko", U"こ"},
    {U"ga", U"が"}, {U"gi", U"ぎ"}, {U"gu", U"ぐ"}, {U"ge", U"げ"}, {U"go", U"ご"},
    {U"sa", U"さ"}, {U"si", U"し"}, {U"shi", U"し"}, {U"su", U"す"}, {U"se", U"せ"}, {U"so", U"そ"},
    {U"za", U"ざ"}, {U"zi", U"じ"}, {U"ji", U"じ"}, {U"zu", U"ず"}, {U"ze", U"ぜ"}, {U"zo", U"ぞ"},
    {U"ta", U"た"}, {U"ti", U"ち"}, {U"chi", U"ち"}, {U"tu", U"つ"}, {U"tsu", U"つ"}, {U"te", U"て"}, {U"to", U"と"},
    {U"da", U"だ"}, {U"di", U"ぢ"}, {U"du", U"づ"}, {U"de", U"で"}, {U"do", U"ど"},
    {U"na", U"な"}, {U"ni", U"に"}, {U"nu", U"ぬ"}, {U"ne", U"ね"}, {U"no", U"の"},
    {U"ha", U"は"}, {U"hi", U"ひ"}, {U"hu", U"ふ"}, {U"fu", U"ふ"}, {U"he", U"へ"}, {U"ho", U"ほ"},
    {U"ba", U"ば"}, {U"bi", U"び"}, {U"bu", U"ぶ"}, {U"be", U"べ"}, {U"bo", U"ぼ"},
    {U"pa", U"ぱ"}, {U"pi", U"ぴ"}, {U"pu", U"ぷ"}, {U"pe", U"ぺ"}, {U"po", U"ぽ"},
    {U"ma", U"ま"}, {U"mi", U"み"}, {U"mu", U"む"}, {U"me", U"め"}, {U"mo", U"も"},
    {U"ya", U"や"}, {U"yu", U"ゆ"}, {U"yo", U"よ"},
    {U"ra", U"ら"}, {U"ri", U"り"}, {U"ru", U"る"}, {U"re", U"れ"}, {U"ro", U"ろ"},
    {U"wa", U"わ"}, {U"wo", U"を"}, {U"wi", U"うぃ"}, {U"we", U"うぇ"}, {U"wu", U"う"},
    {U"kya", U"きゃ"}, {U"kyi", U"きぃ"}, {U"kyu", U"きゅ"}, {U"kye", U"きぇ"}, {U"kyo", U"きょ"},
    {U"gya", U"ぎゃ"}, {U"gyi", U"ぎぃ"}, {U"gyu", U"ぎゅ"}, {U"gye", U"ぎぇ"}, {U"gyo", U"ぎょ"},
    {U"sha", U"しゃ"}, {U"shu", U"しゅ"}, {U"she", U"しぇ"}, {U"sho", U"しょ"},
    {U"sya", U"しゃ"}, {U"syi", U"しぃ"}, {U"syu", U"しゅ"}, {U"sye", U"しぇ"}, {U"syo", U"しょ"},
    {U"ja", U"じゃ"}, {U"ju", U"じゅ"}, {U"je", U"じぇ"}, {U"jo", U"じょ"},
    {U"jya", U"じゃ"}, {U"jyi", U"じぃ"}, {U"jyu", U"じゅ"}, {U"jye", U"じぇ"}, {U"jyo", U"じょ"},
    {U"zya", U"じゃ"}, {U"zyi", U"じぃ"}, {U"zyu", U"じゅ"}, {U"zye", U"じぇ"}, {U"zyo", U"じょ"},
    {U"cha", U"ちゃ"}, {U"chu", U"ちゅ"}, {U"che", U"ちぇ"}, {U"cho", U"ちょ"},
    {U"tya", U"ちゃ"}, {U"tyi", U"ちぃ"}, {U"tyu", U"ちゅ"}, {U"tye", U"ちぇ"}, {U"tyo", U"ちょ"},
    {U"cya", U"ちゃ"}, {U"cyi", U"ちぃ"}, {U"cyu", U"ちゅ"}, {U"cye", U"ちぇ"}, {U"cyo", U"ちょ"},
    {U"dya", U"ぢゃ"}, {U"dyi", U"ぢぃ"}, {U"dyu", U"ぢゅ"}, {U"dye", U"ぢぇ"}, {U"dyo", U"ぢょ"},
    {U"nya", U"にゃ"}, {U"nyi", U"にぃ"}, {U"nyu", U"にゅ"}, {U"nye", U"にぇ"}, {U"nyo", U"にょ"},
    {U"hya", U"ひゃ"}, {U"hyi", U"ひぃ"}, {U"hyu", U"ひゅ"}, {U"hye", U"ひぇ"}, {U"hyo", U"ひょ"},
    {U"bya", U"びゃ"}, {U"byi", U"びぃ"}, {U"byu", U"びゅ"}, {U"bye", U"びぇ"}, {U"byo", U"びょ"},
    {U"pya", U"ぴゃ"}, {U"pyi", U"ぴぃ"}, {U"pyu", U"ぴゅ"}, {U"pye", U"ぴぇ"}, {U"pyo", U"ぴょ"},
    {U"mya", U"みゃ"}, {U"myi", U"みぃ"}, {U"myu", U"みゅ"}, {U"mye", U"みぇ"}, {U"myo", U"みょ"},
    {U"rya", U"りゃ"}, {U"ryi", U"りぃ"}, {U"ryu", U"りゅ"}, {U"rye", U"りぇ"}, {U"ryo", U"りょ"},
    {U"fa", U"ふぁ"}, {U"fi", U"ふぃ"}, {U"fe", U"ふぇ"}, {U"fo", U"ふぉ"}, {U"fyu", U"ふゅ"},
    {U"va", U"ゔぁ"}, {U"vi", U"ゔぃ"}, {U"vu", U"ゔ"}, {U"ve", U"ゔぇ"}, {U"vo", U"ゔぉ"},
    {U"tsa", U"つぁ"}, {U"tsi", U"つぃ"}, {U"tse", U"つぇ"}, {U"tso", U"つぉ"},
    {U"tha", U"てゃ"}, {U"thi", U"てぃ"}, {U"thu", U"てゅ"}, {U"the", U"てぇ"}, {U"tho", U"てょ"},
    {U"dha", U"でゃ"}, {U"dhi", U"でぃ"}, {U"dhu", U"でゅ"}, {U"dhe", U"でぇ"}, {U"dho", U"でょ"},
    {U"twa", U"とぁ"}, {U"twi", U"とぃ"}, {U"twu", U"とぅ"}, {U"twe", U"とぇ"}, {U"two", U"とぉ"},
    {U"dwa", U"どぁ"}, {U"dwi", U"どぃ"}, {U"dwu", U"どぅ"}, {U"dwe", U"どぇ"}, {U"dwo", U"どぉ"},
    {U"kwa", U"くぁ"}, {U"kwi", U"くぃ"}, {U"kwu", U"くぅ"}, {U"kwe", U"くぇ"}, {U"kwo", U"くぉ"},
    {U"qa", U"くぁ"}, {U"qi", U"くぃ"}, {U"qu", U"く"}, {U"qe", U"くぇ"}, {U"qo", U"くぉ"},
    {U"gwa", U"ぐぁ"}, {U"gwi", U"ぐぃ"}, {U"gwu", U"ぐぅ"}, {U"gwe", U"ぐぇ"}, {U"gwo", U"ぐぉ"},
    {U"xa", U"ぁ"}, {U"xi", U"ぃ"}, {U"xu", U"ぅ"}, {U"xe", U"ぇ"}, {U"xo", U"ぉ"},
    {U"la", U"ぁ"}, {U"li", U"ぃ"}, {U"lu", U"ぅ"}, {U"le", U"ぇ"}, {U"lo", U"ぉ"},
    {U"xya", U"ゃ"}, {U"xyu", U"ゅ"}, {U"xyo", U"ょ"}, {U"xwa", U"ゎ"},
    {U"lya", U"ゃ"}, {U"lyu", U"ゅ"}, {U"lyo", U"ょ"}, {U"lwa", U"ゎ"},
    {U"xtu", U"っ"}, {U"xtsu", U"っ"}, {U"ltu", U"っ"}, {U"ltsu", U"っ"},
    {U"xka", U"ゕ"}, {U"xke", U"ゖ"}, {U"lka", U"ゕ"}, {U"lke", U"ゖ"},
    {U"xn", U"ん"}, {U"ln", U"ん"}, {U"ca", U"か"}, {U"ci", U"し"}, {U"cu", U"く"}, {U"ce", U"せ"}, {U"co", U"こ"},
});

bool vowel(char32_t ch) { return std::u32string_view(U"aiueo").find(ch) != std::u32string_view::npos; }
bool consonant(char32_t ch) { return ch >= U'a' && ch <= U'z' && !vowel(ch); }

constexpr std::u32string_view full_kana = U"。「」、・ヲァィゥェォャュョッーアイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワン゛゜";
constexpr std::u32string_view narrow_kana = U"｡｢｣､･ｦｧｨｩｪｫｬｭｮｯｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟ";
static_assert(full_kana.size() == narrow_kana.size());

}  // namespace

KanaReading roman_to_kana(std::u32string_view source, bool finalize) {
    std::u32string input(source);
    for (auto& ch : input) { if (ch >= U'A' && ch <= U'Z') { ch += U'a' - U'A'; } }
    KanaReading result;
    std::size_t index = 0;
    while (index < input.size()) {
        const auto rest = std::u32string_view(input).substr(index);
        const auto ch = rest.front();
        if (ch == U'n') {
            if (rest.size() >= 2 && rest[1] == U'\'') { result.kana += U'ん'; index += 2; continue; }
            if (rest == U"nn") { result.kana += U'ん'; index += 2; continue; }
            if (rest.size() > 2 && rest.starts_with(U"nn") && !vowel(rest[2]) && rest[2] != U'y') {
                result.kana += U'ん'; index += 2; continue;
            }
            if ((rest.size() > 1 && !vowel(rest[1]) && rest[1] != U'y') || (finalize && rest.size() == 1)) {
                result.kana += U'ん'; ++index; continue;
            }
        }
        if (rest.size() > 1 && ch != U'n' && consonant(ch) &&
            (ch == rest[1] || (ch == U't' && rest.starts_with(U"tch")))) {
            result.kana += U'っ'; ++index; continue;
        }
        const Syllable* match = nullptr;
        bool prefix = false;
        for (const auto& item : syllables) {
            if (rest.starts_with(item.roman) && (!match || item.roman.size() > match->roman.size())) { match = &item; }
            if (item.roman.starts_with(rest)) { prefix = true; }
        }
        if (match) { result.kana += match->kana; index += match->roman.size(); continue; }
        if (!finalize && (prefix || rest == U"n")) { result.pending = rest; break; }
        switch (ch) {
        case U'.': result.kana += U'。'; break;
        case U',': result.kana += U'、'; break;
        case U'-': result.kana += U'ー'; break;
        case U'[': result.kana += U'「'; break;
        case U']': result.kana += U'」'; break;
        default: result.kana += ch; break;
        }
        ++index;
    }
    return result;
}

std::u32string to_katakana(std::u32string_view text, bool half_width) {
    std::u32string result;
    for (auto ch : text) {
        if (ch >= U'ぁ' && ch <= U'ゖ') { ch += 0x60; }
        if (!half_width) { result += ch; continue; }
        char32_t mark = 0;
        if (ch == U'ヴ') { ch = U'ウ'; mark = U'ﾞ'; }
        else if ((ch >= U'ガ' && ch <= U'ゴ' && (ch - U'ガ') % 2 == 0) ||
                 (ch >= U'ザ' && ch <= U'ゾ' && (ch - U'ザ') % 2 == 0) ||
                 ch == U'ダ' || ch == U'ヂ' || ch == U'ヅ' || ch == U'デ' || ch == U'ド') {
            --ch; mark = U'ﾞ';
        } else if (ch >= U'ハ' && ch <= U'ポ') {
            const auto remainder = (ch - U'ハ') % 3;
            ch -= remainder;
            if (remainder) { mark = remainder == 1 ? U'ﾞ' : U'ﾟ'; }
        }
        const auto pos = full_kana.find(ch);
        result += pos == std::u32string_view::npos ? ch : narrow_kana[pos];
        if (mark) { result += mark; }
    }
    return result;
}

std::u32string to_full_width_ascii(std::u32string_view text) {
    std::u32string result(text);
    for (auto& ch : result) {
        if (ch == U' ') { ch = U'　'; }
        else if (ch >= U'!' && ch <= U'~') { ch += 0xfee0; }
    }
    return result;
}

void RomajiInput::insert(std::u32string_view text) { raw_.insert(cursor_, text); cursor_ += text.size(); }
bool RomajiInput::erase_previous() { if (!cursor_) { return false; } raw_.erase(--cursor_, 1); return true; }
bool RomajiInput::erase_next() { if (cursor_ == raw_.size()) { return false; } raw_.erase(cursor_, 1); return true; }
void RomajiInput::move_left() noexcept { if (cursor_) { --cursor_; } }
void RomajiInput::move_right() noexcept { if (cursor_ < raw_.size()) { ++cursor_; } }
void RomajiInput::clear() noexcept { raw_.clear(); cursor_ = 0; }
KanaReading RomajiInput::reading(bool finalize) const { return roman_to_kana(raw_, finalize); }
std::size_t RomajiInput::display_cursor() const { return roman_to_kana(std::u32string_view(raw_).substr(0, cursor_)).text().size(); }

}  // namespace windows_live_ime::core
