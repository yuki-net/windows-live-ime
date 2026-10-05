#pragma once

#include "windows_live_ime/ime/pipe_protocol.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace windows_live_ime::ime {

enum class PipeError {
    None,
    Timeout,
    Unavailable,
    Io,
    InvalidResponse,
};

struct PipeExchangeResult {
    std::optional<PipeFrame> response;
    PipeError error{PipeError::None};
};

class NamedPipeClient final {
public:
    explicit NamedPipeClient(std::wstring pipe_name = LR"(\\.\pipe\windows-live-ime)");

    [[nodiscard]] PipeExchangeResult exchange(
        const PipeFrame& request,
        std::chrono::milliseconds timeout) const;

private:
    std::wstring pipe_name_;
};

}  // namespace windows_live_ime::ime
