#pragma once

#include "windows_live_ime/core/model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace windows_live_ime::ime {

inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::size_t kFrameHeaderSize = 32;
inline constexpr std::uint32_t kMaximumPayloadSize = 1024 * 1024;
inline constexpr std::uint32_t kMaximumCandidateCount = 1024;

enum class MessageKind : std::uint16_t {
    Ping = 1,
    Pong = 2,
    Convert = 3,
    ConvertResult = 4,
    Error = 5,
};

struct PipeFrame {
    std::uint16_t version{kProtocolVersion};
    MessageKind kind{MessageKind::Ping};
    std::uint64_t request_id{0};
    std::uint64_t generation_id{0};
    std::uint32_t status{0};
    std::vector<std::uint8_t> payload;
};

enum class ProtocolError {
    None,
    HeaderSize,
    BadMagic,
    UnsupportedVersion,
    InvalidMessageKind,
    PayloadTooLarge,
    LengthMismatch,
    InvalidCandidatePayload,
    InvalidUtf8,
};

[[nodiscard]] std::vector<std::uint8_t> encode_frame(const PipeFrame& frame);
[[nodiscard]] std::optional<std::uint32_t> payload_size_from_header(
    std::span<const std::uint8_t> header,
    ProtocolError* error = nullptr) noexcept;
[[nodiscard]] std::optional<PipeFrame> decode_frame(
    std::span<const std::uint8_t> bytes,
    ProtocolError* error = nullptr) noexcept;

[[nodiscard]] std::vector<std::uint8_t> encode_candidates(std::span<const core::Candidate> candidates);
[[nodiscard]] std::optional<std::vector<core::Candidate>> decode_candidates(
    std::span<const std::uint8_t> payload,
    ProtocolError* error = nullptr) noexcept;

}  // namespace windows_live_ime::ime
