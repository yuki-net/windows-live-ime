#include "windows_live_ime/ime/named_pipe_client.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "windows_live_ime/core/unicode.hpp"
#include <string>

int wmain(int argc, wchar_t* argv[]) {
    using namespace std::chrono_literals;
    std::chrono::milliseconds timeout = 5000ms;
    bool convert = argc == 3 && std::wstring_view(argv[1]) == L"-Convert";
    if (argc == 3 && std::wstring_view(argv[1]) == L"-TimeoutMs") {
        try {
            timeout = std::chrono::milliseconds(std::stoll(argv[2]));
        } catch (const std::exception&) {
            std::wcerr << L"Invalid timeout value.\n";
            return EXIT_FAILURE;
        }
    } else if (argc != 1 && !convert) {
        std::wcerr << L"Usage: ime-ipc-ping.exe [-TimeoutMs milliseconds]\n";
        return EXIT_FAILURE;
    }

    windows_live_ime::ime::NamedPipeClient client;
    if (convert) {
        std::u32string input;
        for (const wchar_t c : std::wstring_view(argv[2])) input.push_back(c);
        const auto utf8 = windows_live_ime::core::encode_utf8(input);
        const auto response = client.exchange({.kind=windows_live_ime::ime::MessageKind::Convert,
            .request_id=42,.generation_id=7,.payload=std::vector<std::uint8_t>(utf8.begin(),utf8.end())}, timeout);
        if (!response.response || response.response->kind != windows_live_ime::ime::MessageKind::ConvertResult ||
            response.response->status || response.response->request_id != 42 || response.response->generation_id != 7) {
            std::wcerr << L"Conversion failed (error " << static_cast<int>(response.error) << L").\n"; return EXIT_FAILURE;
        }
        const auto candidates=windows_live_ime::ime::decode_candidates(response.response->payload);
        if (!candidates || candidates->empty()) { std::wcerr << L"No conversion candidates.\n"; return EXIT_FAILURE; }
        bool dictionary_candidate=false;
        for (const auto& candidate : *candidates) {
            std::wstring text;
            for (const auto c : candidate.text) {
                if (c <= 0xffff) text.push_back(static_cast<wchar_t>(c));
                else { const auto scalar=c-0x10000; text.push_back(static_cast<wchar_t>(0xd800+(scalar>>10))); text.push_back(static_cast<wchar_t>(0xdc00+(scalar&0x3ff))); }
                if (c >= 0x4e00 && c <= 0x9fff) dictionary_candidate=true;
            }
            std::wcout << text << L"\n";
        }
        return dictionary_candidate ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    const auto result = client.exchange(
        windows_live_ime::ime::PipeFrame{.kind = windows_live_ime::ime::MessageKind::Ping},
        timeout);
    if (result.error != windows_live_ime::ime::PipeError::None ||
        !result.response.has_value() ||
        result.response->kind != windows_live_ime::ime::MessageKind::Pong) {
        std::wcerr << L"Engine host ping failed (error " << static_cast<int>(result.error) << L").\n";
        return EXIT_FAILURE;
    }

    std::wcout << L"Engine host is healthy.\n";
    return EXIT_SUCCESS;
}
