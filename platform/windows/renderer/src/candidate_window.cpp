#include "windows_live_ime/renderer/candidate_window.hpp"

#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <stdexcept>

namespace windows_live_ime::renderer {
namespace {
constexpr wchar_t kWindowClass[] = L"LiveImeCandidateWindow";
constexpr float kWidth = 320.0f;
constexpr float kRowHeight = 32.0f;
constexpr std::size_t kPageSize = 9;
}

CandidateWindow::CandidateWindow(HINSTANCE instance) {
    WNDCLASSEXW cls{sizeof(cls)};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { return; }
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        kWindowClass, L"Live IME", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!window_) { return; }
    const DWORD rounded = 2;
    DwmSetWindowAttribute(window_, 33, &rounded, sizeof(rounded));
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_))) { return; }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&write_factory_)))) { return; }
    write_factory_->CreateTextFormat(L"Yu Gothic UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"ja-JP", &format_);
}

CandidateWindow::~CandidateWindow() {
    if (window_) { DestroyWindow(window_); }
    discard_target();
    if (format_) { format_->Release(); }
    if (write_factory_) { write_factory_->Release(); }
    if (factory_) { factory_->Release(); }
}

void CandidateWindow::discard_target() noexcept { if (target_) { target_->Release(); target_ = nullptr; } }
void CandidateWindow::hide() noexcept { if (window_) { ShowWindow(window_, SW_HIDE); } }
void CandidateWindow::set_selection_handler(std::function<void(std::size_t)> handler) { selection_handler_ = std::move(handler); }

void CandidateWindow::show(const std::vector<std::wstring>& candidates, std::size_t selected,
                           const RECT& caret, bool engine_available) {
    if (!window_ || candidates.empty()) { hide(); return; }
    candidates_ = candidates;
    selected_ = std::min(selected, candidates_.size() - 1);
    page_start_ = selected_ / kPageSize * kPageSize;
    available_ = engine_available;
    const auto monitor = MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    scale_ = static_cast<float>(GetDpiForWindow(window_)) / 96.0f;
    const auto rows = std::min(kPageSize, candidates_.size() - page_start_);
    const auto width = static_cast<int>(kWidth * scale_);
    const auto height = static_cast<int>((static_cast<float>(rows) * kRowHeight + 40.0f) * scale_);
    const auto x = std::clamp(caret.left, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - width));
    auto y = caret.bottom + 4;
    if (y + height > info.rcWork.bottom) { y = std::max(info.rcWork.top, caret.top - height - 4); }
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    std::wstring accessible = L"Live IME candidates";
    for (const auto& text : candidates_) { accessible += L" | " + text; }
    SetWindowTextW(window_, accessible.c_str());
    discard_target();
    InvalidateRect(window_, nullptr, FALSE);
}

void CandidateWindow::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(window_, &ps);
    if (!factory_ || !format_) { EndPaint(window_, &ps); return; }
    if (!target_) {
        RECT client{}; GetClientRect(window_, &client);
        factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(window_, D2D1::SizeU(client.right, client.bottom)), &target_);
    }
    if (!target_) { EndPaint(window_, &ps); return; }
    target_->SetDpi(96.0f * scale_, 96.0f * scale_);
    ID2D1SolidColorBrush* text_brush = nullptr;
    ID2D1SolidColorBrush* selected_brush = nullptr;
    target_->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.09f), &text_brush);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.88f, 0.94f, 1.0f), &selected_brush);
    if (text_brush && selected_brush) {
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0.98f, 0.98f, 0.98f));
        const auto rows = std::min(kPageSize, candidates_.size() - page_start_);
        for (std::size_t row = 0; row < rows; ++row) {
            const auto index = page_start_ + row;
            const auto top = 6.0f + static_cast<float>(row) * kRowHeight;
            if (index == selected_) { target_->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(4.0f, top, kWidth - 4.0f, top + kRowHeight), 4.0f, 4.0f), selected_brush); }
            const auto text = std::to_wstring(row + 1) + L"  " + candidates_[index];
            target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format_,
                D2D1::RectF(12.0f, top + 5.0f, kWidth - 12.0f, top + kRowHeight), text_brush,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        const std::wstring footer = available_ ? L"Live IME  ·  ↑↓ 選択 / Enter 確定" : L"Live IME  ·  エンジン未接続：かな入力";
        target_->DrawTextW(footer.c_str(), static_cast<UINT32>(footer.size()), format_,
            D2D1::RectF(12.0f, 10.0f + static_cast<float>(rows) * kRowHeight, kWidth - 8.0f,
                        38.0f + static_cast<float>(rows) * kRowHeight), text_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (target_->EndDraw() == D2DERR_RECREATE_TARGET) { discard_target(); }
    }
    if (text_brush) { text_brush->Release(); }
    if (selected_brush) { selected_brush->Release(); }
    EndPaint(window_, &ps);
}

LRESULT CALLBACK CandidateWindow::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<CandidateWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
        if (message == WM_MOUSEACTIVATE) { return MA_NOACTIVATE; }
        if (message == WM_PAINT) { self->paint(); return 0; }
        if (message == WM_LBUTTONUP) {
            const auto y = static_cast<float>(GET_Y_LPARAM(lparam)) / self->scale_ - 6.0f;
            if (y >= 0.0f) {
                const auto row = static_cast<std::size_t>(y / kRowHeight);
                const auto index = self->page_start_ + row;
                if (row < kPageSize && index < self->candidates_.size() && self->selection_handler_) {
                    self->selection_handler_(index);
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace windows_live_ime::renderer
