#include "windows_live_ime/ime/mode_button.hpp"
#include "windows_live_ime/ime/text_service.hpp"
#include "windows_live_ime/ime/com_module.hpp"
#include <iostream>

// Only the menu is under test; no TSF service or user document is activated.
namespace windows_live_ime::ime {
extern const CLSID CLSID_WindowsLiveImeTextService={};
void module_object_created() noexcept {}
void module_object_destroyed() noexcept {}
HINSTANCE module_instance() noexcept { return GetModuleHandleW(nullptr); }
void TextService::set_input_mode(bool) {}
void TextService::toggle_input_mode() {}
}
namespace {
bool menu_shown=false;
void CALLBACK cancel_menu(HWND,UINT,UINT_PTR timer,DWORD) {
    GUITHREADINFO info{sizeof(info)};
    if (GetGUIThreadInfo(GetCurrentThreadId(),&info) && (info.flags & GUI_INMENUMODE)) {
        menu_shown=true;
        SendMessageW(info.hwndMenuOwner,WM_CANCELMODE,0,0);
    }
    KillTimer(nullptr,timer);
    // Also prevents a failed popup from leaving a test blocked indefinitely.
    EndMenu();
}
}
int main() {
    using namespace windows_live_ime::ime;
    auto* button=new ModeButton(nullptr);
    const auto timer=SetTimer(nullptr,0,300,cancel_menu);
    if (!timer) { button->Release(); return 1; }
    POINT point{120,120};
    const auto result=button->OnClick(TF_LBI_CLK_RIGHT,point,nullptr);
    KillTimer(nullptr,timer);
    button->Release();
    if (FAILED(result) || !menu_shown) { std::cerr << "Right-click did not enter menu mode.\n"; return 1; }
    std::cout << "Actual right-click callback opened a native popup menu.\n";
    return 0;
}
