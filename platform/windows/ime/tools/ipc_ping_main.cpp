#include "windows_live_ime/ime/named_pipe_client.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>

int wmain(int argc, wchar_t* argv[]) {
    using namespace std::chrono_literals;
    std::chrono::milliseconds timeout = 5000ms;
    if (argc == 3 && std::wstring_view(argv[1]) == L"-TimeoutMs") {
        try {
            timeout = std::chrono::milliseconds(std::stoll(argv[2]));
        } catch (const std::exception&) {
            std::wcerr << L"Invalid timeout value.\n";
            return EXIT_FAILURE;
        }
    } else if (argc != 1) {
        std::wcerr << L"Usage: ime-ipc-ping.exe [-TimeoutMs milliseconds]\n";
        return EXIT_FAILURE;
    }

    windows_live_ime::ime::NamedPipeClient client;
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
