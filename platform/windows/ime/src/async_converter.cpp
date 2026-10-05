#include "windows_live_ime/ime/async_converter.hpp"

#include "windows_live_ime/core/unicode.hpp"
#include "windows_live_ime/ime/named_pipe_client.hpp"

namespace windows_live_ime::ime {

AsyncConverter::AsyncConverter(HWND receiver)
    : receiver_(receiver), worker_([this](std::stop_token stop) { run(stop); }) {}

AsyncConverter::~AsyncConverter() {
    worker_.request_stop();
    changed_.notify_all();
    worker_.join();
}

void AsyncConverter::submit(core::ConversionRequest request) {
    {
        std::lock_guard lock(mutex_);
        pending_ = std::move(request);
    }
    changed_.notify_one();
}

std::optional<core::ConversionResponse> AsyncConverter::take_result() {
    std::lock_guard lock(mutex_);
    auto result = std::move(result_);
    result_.reset();
    return result;
}

void AsyncConverter::run(std::stop_token stop) {
    NamedPipeClient client;
    while (!stop.stop_requested()) {
        core::ConversionRequest request;
        {
            std::unique_lock lock(mutex_);
            if (!changed_.wait(lock, stop, [this] { return pending_.has_value(); })) { return; }
            request = std::move(*pending_);
            pending_.reset();
        }
        core::ConversionResponse converted{request.request_id, request.generation_id, {}};
        try {
            const auto utf8 = core::encode_utf8(request.input);
            PipeFrame frame;
            frame.kind = MessageKind::Convert;
            frame.request_id = request.request_id;
            frame.generation_id = request.generation_id;
            frame.payload.assign(utf8.begin(), utf8.end());
            const auto exchange = client.exchange(frame, std::chrono::milliseconds(3000));
            if (exchange.response && exchange.response->kind == MessageKind::ConvertResult &&
                exchange.response->status == 0 && exchange.response->request_id == request.request_id &&
                exchange.response->generation_id == request.generation_id) {
                auto candidates = decode_candidates(exchange.response->payload);
                if (candidates) { converted.candidates = std::move(*candidates); }
            }
        } catch (...) {
            // Kana already appears in the application. A failed engine request
            // must never terminate the application that loaded this DLL.
        }
        {
            std::lock_guard lock(mutex_);
            if (stop.stop_requested()) { return; }
            if (pending_ && pending_->generation_id > request.generation_id) { continue; }
            result_ = std::move(converted);
        }
        PostMessageW(receiver_, kConversionReadyMessage, 0, 0);
    }
}

}  // namespace windows_live_ime::ime
