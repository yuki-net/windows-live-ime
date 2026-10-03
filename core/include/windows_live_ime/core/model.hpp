#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace windows_live_ime::core {

using RequestId = std::uint64_t;
using GenerationId = std::uint64_t;

enum class InputMode {
    Japanese,
    Latin,
};

struct InputState {
    bool enabled{false};
    InputMode mode{InputMode::Japanese};
};

struct CompositionState {
    // Text is represented as Unicode scalar values. cursor is an index into text.
    std::u32string text;
    std::size_t cursor{0};
};

struct Candidate {
    std::u32string text;
    std::u32string annotation;
};

enum class KeyAction {
    PassThrough,
    InsertText,
    DeletePrevious,
    MoveCursorLeft,
    MoveCursorRight,
    BeginCandidateSelection,
    Commit,
    Cancel,
    ToggleInputMode,
};

struct ConversionRequest {
    RequestId request_id{0};
    GenerationId generation_id{0};
    std::u32string input;
};

struct ConversionResponse {
    RequestId request_id{0};
    GenerationId generation_id{0};
    std::vector<Candidate> candidates;
};

}  // namespace windows_live_ime::core
