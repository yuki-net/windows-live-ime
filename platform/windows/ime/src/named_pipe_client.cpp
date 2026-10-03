#include "windows_live_ime/ime/named_pipe_client.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <memory>
#include <thread>
#include <vector>

namespace windows_live_ime::ime {
namespace {

using Clock = std::chrono::steady_clock;

DWORD remaining_milliseconds(Clock::time_point deadline) noexcept {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
    if (remaining.count() <= 0) {
        return 0;
    }
    return static_cast<DWORD>(std::min<std::int64_t>(remaining.count(), MAXDWORD));
}

struct PendingIo final {
    OVERLAPPED overlapped{};
};

DWORD WINAPI reap_pending_io(LPVOID context) {
    auto* pending = static_cast<PendingIo*>(context);
    if (WaitForSingleObject(pending->overlapped.hEvent, 5000) == WAIT_OBJECT_0) {
        CloseHandle(pending->overlapped.hEvent);
        delete pending;
    }
    // If cancellation does not complete promptly, retain the kernel-owned
    // OVERLAPPED and event rather than freeing memory still used by Windows.
    return 0;
}

HANDLE connect_pipe(const std::wstring& name, Clock::time_point deadline, PipeError& error) noexcept {
    while (remaining_milliseconds(deadline) != 0) {
        const auto remaining = remaining_milliseconds(deadline);
        if (!WaitNamedPipeW(name.c_str(), remaining)) {
            const auto last_error = GetLastError();
            if (last_error == ERROR_SEM_TIMEOUT || last_error == ERROR_FILE_NOT_FOUND ||
                last_error == ERROR_PIPE_BUSY) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }
            error = PipeError::Unavailable;
            return INVALID_HANDLE_VALUE;
        }

        HANDLE pipe = CreateFileW(
            name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_BYTE;
            if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
                CloseHandle(pipe);
                error = PipeError::Io;
                return INVALID_HANDLE_VALUE;
            }
            error = PipeError::None;
            return pipe;
        }

        const auto last_error = GetLastError();
        if (last_error == ERROR_PIPE_BUSY || last_error == ERROR_FILE_NOT_FOUND) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }
        error = PipeError::Unavailable;
        return INVALID_HANDLE_VALUE;
    }

    error = PipeError::Timeout;
    return INVALID_HANDLE_VALUE;
}

PipeError transfer_exact(
    HANDLE pipe,
    std::uint8_t* buffer,
    std::size_t size,
    bool write,
    Clock::time_point deadline) {
    std::size_t offset = 0;
    while (offset < size) {
        const auto remaining = remaining_milliseconds(deadline);
        if (remaining == 0) {
            return PipeError::Timeout;
        }

        auto pending = std::make_unique<PendingIo>();
        pending->overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (pending->overlapped.hEvent == nullptr) {
            return PipeError::Io;
        }

        const auto chunk_size = static_cast<DWORD>(std::min<std::size_t>(size - offset, MAXDWORD));
        DWORD transferred = 0;
        const BOOL completed = write
            ? WriteFile(pipe, buffer + offset, chunk_size, &transferred, &pending->overlapped)
            : ReadFile(pipe, buffer + offset, chunk_size, &transferred, &pending->overlapped);

        if (!completed) {
            const auto last_error = GetLastError();
            if (last_error != ERROR_IO_PENDING) {
                CloseHandle(pending->overlapped.hEvent);
                return PipeError::Io;
            }

            const auto wait_result = WaitForSingleObject(pending->overlapped.hEvent, remaining);
            if (wait_result == WAIT_TIMEOUT) {
                CancelIoEx(pipe, &pending->overlapped);
                auto* cleanupState = pending.release();
                if (!QueueUserWorkItem(reap_pending_io, cleanupState, WT_EXECUTELONGFUNCTION)) {
                    if (WaitForSingleObject(cleanupState->overlapped.hEvent, 5000) == WAIT_OBJECT_0) {
                        CloseHandle(cleanupState->overlapped.hEvent);
                        delete cleanupState;
                    }
                    // Otherwise keep the state allocated until process teardown;
                    // the pipe handle is closed by the caller below.
                }
                return PipeError::Timeout;
            }
            if (wait_result != WAIT_OBJECT_0 ||
                !GetOverlappedResult(pipe, &pending->overlapped, &transferred, FALSE)) {
                CloseHandle(pending->overlapped.hEvent);
                return PipeError::Io;
            }
        }

        CloseHandle(pending->overlapped.hEvent);
        if (transferred == 0) {
            return PipeError::Io;
        }
        offset += transferred;
    }
    return PipeError::None;
}

}  // namespace

NamedPipeClient::NamedPipeClient(std::wstring pipe_name)
    : pipe_name_(std::move(pipe_name)) {}

PipeExchangeResult NamedPipeClient::exchange(
    const PipeFrame& request,
    std::chrono::milliseconds timeout) const {
    if (timeout.count() <= 0) {
        return {.response = std::nullopt, .error = PipeError::Timeout};
    }

    std::vector<std::uint8_t> request_bytes;
    try {
        request_bytes = encode_frame(request);
    } catch (const std::exception&) {
        return {.response = std::nullopt, .error = PipeError::InvalidResponse};
    }

    const auto deadline = Clock::now() + timeout;
    PipeError connect_error = PipeError::None;
    HANDLE pipe = connect_pipe(pipe_name_, deadline, connect_error);
    if (pipe == INVALID_HANDLE_VALUE) {
        return {.response = std::nullopt, .error = connect_error};
    }

    const auto write_error = transfer_exact(
        pipe,
        request_bytes.data(),
        request_bytes.size(),
        true,
        deadline);
    if (write_error != PipeError::None) {
        CloseHandle(pipe);
        return {.response = std::nullopt, .error = write_error};
    }

    std::array<std::uint8_t, kFrameHeaderSize> header{};
    const auto header_error = transfer_exact(pipe, header.data(), header.size(), false, deadline);
    if (header_error != PipeError::None) {
        CloseHandle(pipe);
        return {.response = std::nullopt, .error = header_error};
    }

    ProtocolError protocol_error = ProtocolError::None;
    const auto payload_size = payload_size_from_header(header, &protocol_error);
    if (!payload_size.has_value()) {
        CloseHandle(pipe);
        return {.response = std::nullopt, .error = PipeError::InvalidResponse};
    }

    std::vector<std::uint8_t> response_bytes(header.begin(), header.end());
    response_bytes.resize(kFrameHeaderSize + *payload_size);
    if (*payload_size != 0) {
        const auto payload_error = transfer_exact(
            pipe,
            response_bytes.data() + kFrameHeaderSize,
            *payload_size,
            false,
            deadline);
        if (payload_error != PipeError::None) {
            CloseHandle(pipe);
            return {.response = std::nullopt, .error = payload_error};
        }
    }

    CloseHandle(pipe);
    auto response = decode_frame(response_bytes, &protocol_error);
    if (!response.has_value()) {
        return {.response = std::nullopt, .error = PipeError::InvalidResponse};
    }
    return {.response = std::move(response), .error = PipeError::None};
}

}  // namespace windows_live_ime::ime
