#pragma once

#include "windows_live_ime/core/model.hpp"

#include <Windows.h>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace windows_live_ime::ime {

inline constexpr UINT kConversionReadyMessage = WM_APP + 0x41;

// No TSF or other apartment-bound COM objects cross this boundary.
// Pending work is replaced by the newest input while a request is in flight.
class AsyncConverter final {
public:
    explicit AsyncConverter(HWND receiver);
    ~AsyncConverter();
    void submit(core::ConversionRequest request);
    [[nodiscard]] std::optional<core::ConversionResponse> take_result();

private:
    void run(std::stop_token stop);
    HWND receiver_;
    std::mutex mutex_;
    std::condition_variable_any changed_;
    std::optional<core::ConversionRequest> pending_;
    std::optional<core::ConversionResponse> result_;
    std::jthread worker_;
};

}  // namespace windows_live_ime::ime
