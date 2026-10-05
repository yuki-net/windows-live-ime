#include "windows_live_ime/ime/pipe_protocol.hpp"

#include "windows_live_ime/core/unicode.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace windows_live_ime::ime {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'W', 'I', 'M', 'E'};
constexpr std::uint32_t kMaximumCandidateStringSize = kMaximumPayloadSize;

void set_error(ProtocolError* output, ProtocolError error) noexcept {
    if (output != nullptr) {
        *output = error;
    }
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xFF));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned int offset = 0; offset < 32; offset += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> offset) & 0xFF));
    }
}

void append_u64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (unsigned int offset = 0; offset < 64; offset += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> offset) & 0xFF));
    }
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (unsigned int byte = 0; byte < 4; ++byte) {
        value |= static_cast<std::uint32_t>(bytes[offset + byte]) << (byte * 8);
    }
    return value;
}

std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (unsigned int byte = 0; byte < 8; ++byte) {
        value |= static_cast<std::uint64_t>(bytes[offset + byte]) << (byte * 8);
    }
    return value;
}

bool is_valid_kind(std::uint16_t raw_kind) noexcept {
    return raw_kind >= static_cast<std::uint16_t>(MessageKind::Ping) &&
           raw_kind <= static_cast<std::uint16_t>(MessageKind::Error);
}

bool append_candidate_string(std::vector<std::uint8_t>& output, std::u32string_view text) {
    const auto utf8 = core::encode_utf8(text);
    if (utf8.size() > kMaximumCandidateStringSize ||
        output.size() + sizeof(std::uint32_t) + utf8.size() > kMaximumPayloadSize) {
        return false;
    }
    append_u32(output, static_cast<std::uint32_t>(utf8.size()));
    output.insert(output.end(), utf8.begin(), utf8.end());
    return true;
}

std::optional<std::u32string> read_candidate_string(
    std::span<const std::uint8_t> payload,
    std::size_t& offset) noexcept {
    if (payload.size() - offset < sizeof(std::uint32_t)) {
        return std::nullopt;
    }

    const auto byte_count = read_u32(payload, offset);
    offset += sizeof(std::uint32_t);
    if (byte_count > kMaximumCandidateStringSize || payload.size() - offset < byte_count) {
        return std::nullopt;
    }

    const auto* begin = reinterpret_cast<const char*>(payload.data() + offset);
    auto decoded = core::decode_utf8(std::string_view(begin, byte_count));
    offset += byte_count;
    return decoded;
}

}  // namespace

std::vector<std::uint8_t> encode_frame(const PipeFrame& frame) {
    if (frame.payload.size() > kMaximumPayloadSize) {
        throw std::invalid_argument("Named Pipe payload exceeds the protocol limit.");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(kFrameHeaderSize + frame.payload.size());
    bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
    append_u16(bytes, frame.version);
    append_u16(bytes, static_cast<std::uint16_t>(frame.kind));
    append_u32(bytes, static_cast<std::uint32_t>(frame.payload.size()));
    append_u64(bytes, frame.request_id);
    append_u64(bytes, frame.generation_id);
    append_u32(bytes, frame.status);
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

std::optional<std::uint32_t> payload_size_from_header(
    std::span<const std::uint8_t> header,
    ProtocolError* error) noexcept {
    set_error(error, ProtocolError::None);
    if (header.size() != kFrameHeaderSize) {
        set_error(error, ProtocolError::HeaderSize);
        return std::nullopt;
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), header.begin())) {
        set_error(error, ProtocolError::BadMagic);
        return std::nullopt;
    }
    if (read_u16(header, 4) != kProtocolVersion) {
        set_error(error, ProtocolError::UnsupportedVersion);
        return std::nullopt;
    }
    if (!is_valid_kind(read_u16(header, 6))) {
        set_error(error, ProtocolError::InvalidMessageKind);
        return std::nullopt;
    }

    const auto payload_size = read_u32(header, 8);
    if (payload_size > kMaximumPayloadSize) {
        set_error(error, ProtocolError::PayloadTooLarge);
        return std::nullopt;
    }
    return payload_size;
}

std::optional<PipeFrame> decode_frame(
    std::span<const std::uint8_t> bytes,
    ProtocolError* error) noexcept {
    if (bytes.size() < kFrameHeaderSize) {
        set_error(error, ProtocolError::HeaderSize);
        return std::nullopt;
    }

    const auto header = bytes.first(kFrameHeaderSize);
    const auto payload_size = payload_size_from_header(header, error);
    if (!payload_size.has_value()) {
        return std::nullopt;
    }
    if (bytes.size() != kFrameHeaderSize + *payload_size) {
        set_error(error, ProtocolError::LengthMismatch);
        return std::nullopt;
    }

    PipeFrame frame;
    frame.version = read_u16(header, 4);
    frame.kind = static_cast<MessageKind>(read_u16(header, 6));
    frame.request_id = read_u64(header, 12);
    frame.generation_id = read_u64(header, 20);
    frame.status = read_u32(header, 28);
    frame.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kFrameHeaderSize), bytes.end());
    set_error(error, ProtocolError::None);
    return frame;
}

std::vector<std::uint8_t> encode_candidates(std::span<const core::Candidate> candidates) {
    if (candidates.size() > kMaximumCandidateCount) {
        throw std::invalid_argument("Candidate count exceeds the protocol limit.");
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(sizeof(std::uint32_t) + candidates.size() * 16);
    append_u32(payload, static_cast<std::uint32_t>(candidates.size()));
    for (const auto& candidate : candidates) {
        if (!append_candidate_string(payload, candidate.text) ||
            !append_candidate_string(payload, candidate.annotation)) {
            throw std::invalid_argument("Candidate payload exceeds the protocol limit.");
        }
    }
    return payload;
}

std::optional<std::vector<core::Candidate>> decode_candidates(
    std::span<const std::uint8_t> payload,
    ProtocolError* error) noexcept {
    set_error(error, ProtocolError::None);
    if (payload.size() < sizeof(std::uint32_t) || payload.size() > kMaximumPayloadSize) {
        set_error(error, ProtocolError::InvalidCandidatePayload);
        return std::nullopt;
    }

    const auto count = read_u32(payload, 0);
    if (count > kMaximumCandidateCount) {
        set_error(error, ProtocolError::InvalidCandidatePayload);
        return std::nullopt;
    }

    std::vector<core::Candidate> candidates;
    candidates.reserve(count);
    std::size_t offset = sizeof(std::uint32_t);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto text = read_candidate_string(payload, offset);
        const auto annotation = read_candidate_string(payload, offset);
        if (!text.has_value() || !annotation.has_value()) {
            set_error(error, ProtocolError::InvalidUtf8);
            return std::nullopt;
        }
        candidates.push_back(core::Candidate{*text, *annotation});
    }

    if (offset != payload.size()) {
        set_error(error, ProtocolError::LengthMismatch);
        return std::nullopt;
    }
    return candidates;
}

}  // namespace windows_live_ime::ime
