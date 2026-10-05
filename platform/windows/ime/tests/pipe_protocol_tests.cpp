#include "windows_live_ime/ime/pipe_protocol.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_frame_round_trip() {
    using namespace windows_live_ime::ime;
    PipeFrame request{
        .version = kProtocolVersion,
        .kind = MessageKind::Convert,
        .request_id = 100,
        .generation_id = 102,
        .status = 0,
        .payload = {0xE3, 0x81, 0x8B, 0xE3, 0x81, 0xAA},
    };
    const auto encoded = encode_frame(request);

    require(encoded.size() == kFrameHeaderSize + request.payload.size(), "frame size should match header and payload");
    require(encoded[0] == 'W' && encoded[1] == 'I' && encoded[2] == 'M' && encoded[3] == 'E',
            "frame magic should be stable");
    const auto decoded = decode_frame(encoded);
    require(decoded.has_value(), "encoded frame should decode");
    require(decoded->kind == request.kind, "message kind should round-trip");
    require(decoded->request_id == request.request_id, "request id should round-trip");
    require(decoded->generation_id == request.generation_id, "generation id should round-trip");
    require(decoded->payload == request.payload, "UTF-8 payload bytes should round-trip");
}

void test_frame_rejects_invalid_data() {
    using namespace windows_live_ime::ime;
    PipeFrame frame{.kind = MessageKind::Ping};
    auto encoded = encode_frame(frame);

    encoded[0] = 'X';
    ProtocolError error = ProtocolError::None;
    require(!decode_frame(encoded, &error).has_value(), "bad magic must be rejected");
    require(error == ProtocolError::BadMagic, "bad magic should report the right error");

    encoded = encode_frame(frame);
    encoded[4] = 2;
    require(!decode_frame(encoded, &error).has_value(), "unsupported protocol version must be rejected");
    require(error == ProtocolError::UnsupportedVersion, "version mismatch should report the right error");

    encoded = encode_frame(frame);
    encoded.push_back(0);
    require(!decode_frame(encoded, &error).has_value(), "trailing bytes must be rejected");
    require(error == ProtocolError::LengthMismatch, "length mismatch should report the right error");
}

void test_candidate_payload_round_trip() {
    using namespace windows_live_ime;
    const std::array candidates{
        core::Candidate{U"仮名", U"かな"},
        core::Candidate{U"漢字", U"かんじ"},
    };
    const auto encoded = ime::encode_candidates(candidates);
    const auto decoded = ime::decode_candidates(encoded);

    require(decoded.has_value(), "encoded candidates should decode");
    require(decoded->size() == candidates.size(), "candidate count should round-trip");
    require((*decoded)[0].text == candidates[0].text, "candidate text should round-trip");
    require((*decoded)[0].annotation == candidates[0].annotation, "candidate annotation should round-trip");
    require((*decoded)[1].text == candidates[1].text, "second candidate should round-trip");
}

}  // namespace

int main() {
    try {
        test_frame_round_trip();
        test_frame_rejects_invalid_data();
        test_candidate_payload_round_trip();
        std::cout << "IPC protocol tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "IPC protocol test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
