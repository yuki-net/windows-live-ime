#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include <string>
#include <vector>

namespace windows_live_ime::renderer {

class CandidateWindow final {
public:
    explicit CandidateWindow(HINSTANCE instance);
    ~CandidateWindow();
    void show(const std::vector<std::wstring>& candidates, std::size_t selected,
              const RECT& caret, bool engine_available);
    void hide() noexcept;
    void set_selection_handler(std::function<void(std::size_t)> handler);

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void discard_target() noexcept;
    HWND window_{nullptr};
    ID2D1Factory* factory_{nullptr};
    IDWriteFactory* write_factory_{nullptr};
    IDWriteTextFormat* format_{nullptr};
    ID2D1HwndRenderTarget* target_{nullptr};
    std::vector<std::wstring> candidates_;
    std::size_t selected_{0};
    std::size_t page_start_{0};
    bool available_{true};
    float scale_{1.0f};
    std::function<void(std::size_t)> selection_handler_;
};

}  // namespace windows_live_ime::renderer
