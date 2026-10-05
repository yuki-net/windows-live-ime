#include "windows_live_ime/ime/mode_button.hpp"
#include "windows_live_ime/ime/text_service.hpp"
#include "windows_live_ime/ime/com_module.hpp"

#include <oleauto.h>
#include <olectl.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace windows_live_ime::ime {
namespace {
// Windows' standard input-mode language bar item identifier.
constexpr GUID kInputModeItem = {0x2c77a81e, 0x41cc, 0x4178, {0xa3, 0xa7, 0x5f, 0x8a, 0x98, 0x75, 0x68, 0xe6}};
}

ModeButton::ModeButton(TextService* service) noexcept : service_(service) { module_object_created(); }
ModeButton::~ModeButton() { if (sink_) { sink_->Release(); } module_object_destroyed(); }
void ModeButton::detach() noexcept { service_ = nullptr; }
void ModeButton::refresh() { if (sink_) { sink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP); } }
STDMETHODIMP ModeButton::QueryInterface(REFIID iid, void** object) {
    if (!object) { return E_POINTER; }
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfLangBarItem || iid == IID_ITfLangBarItemButton) {
        *object = static_cast<ITfLangBarItemButton*>(this);
    } else if (iid == IID_ITfSource) { *object = static_cast<ITfSource*>(this); }
    else { return E_NOINTERFACE; }
    AddRef(); return S_OK;
}
STDMETHODIMP_(ULONG) ModeButton::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&references_)); }
STDMETHODIMP_(ULONG) ModeButton::Release() {
    const auto count = InterlockedDecrement(&references_);
    if (!count) { delete this; }
    return static_cast<ULONG>(count);
}
STDMETHODIMP ModeButton::GetInfo(TF_LANGBARITEMINFO* info) {
    if (!info) { return E_POINTER; }
    *info = {};
    info->clsidService = CLSID_WindowsLiveImeTextService;
    info->guidItem = kInputModeItem;
    info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
    wcscpy_s(info->szDescription, L"Live IME");
    return S_OK;
}
STDMETHODIMP ModeButton::GetStatus(DWORD* status) {
    if (!status) { return E_POINTER; }
    *status = hidden_ ? TF_LBI_STATUS_HIDDEN : 0;
    return S_OK;
}
STDMETHODIMP ModeButton::Show(BOOL show) {
    hidden_ = !show;
    if (sink_) { sink_->OnUpdate(TF_LBI_STATUS); }
    return S_OK;
}
STDMETHODIMP ModeButton::GetTooltipString(BSTR* text) {
    if (!text) { return E_POINTER; }
    *text = SysAllocString(service_ && service_->japanese_mode()
        ? L"Live IME：日本語入力（左クリックで英数／右クリックで設定）" : L"Live IME：英数入力（左クリックで日本語／右クリックで設定）");
    return *text ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP ModeButton::OnClick(TfLBIClick click, POINT point, const RECT*) {
    if (service_ && click == TF_LBI_CLK_LEFT) { service_->toggle_input_mode(); }
    if (click == TF_LBI_CLK_RIGHT) {
        const auto menu = CreatePopupMenu();
        if (!menu) return HRESULT_FROM_WIN32(GetLastError());
        const bool japanese = service_ && service_->japanese_mode();
        AppendMenuW(menu, MF_STRING | (japanese ? MF_CHECKED : 0), 1, L"ひらがな");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 3, L"（未実装）全角カタカナ");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 4, L"（未実装）全角英数字");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 5, L"（未実装）半角カタカナ");
        AppendMenuW(menu, MF_STRING | (japanese ? 0 : MF_CHECKED), 2, L"半角英数字");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 6, L"（未実装）単語の追加");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 7, L"（未実装）プライベートモード（オフ）");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 8, L"設定");
        // BTN_MENU provides a language-bar drop-down, not a right-click menu.
        // The tray's right-click callback must explicitly display its popup.
        const auto owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Live IME menu",
            WS_POPUP, point.x, point.y, 0, 0, GetForegroundWindow(), nullptr, module_instance(), nullptr);
        if (!owner) { DestroyMenu(menu); return HRESULT_FROM_WIN32(GetLastError()); }
        const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            point.x, point.y, owner, nullptr);
        DestroyMenu(menu); DestroyWindow(owner);
        if (command) return OnMenuSelect(command);
    }
    return S_OK;
}
STDMETHODIMP ModeButton::InitMenu(ITfMenu* menu) {
    if (!menu) { return E_POINTER; }
    const auto japanese = service_ && service_->japanese_mode();
    const auto add = [menu](UINT id, DWORD flags, std::wstring_view label) {
        return menu->AddMenuItem(id, flags, nullptr, nullptr, label.data(), static_cast<ULONG>(label.size()), nullptr);
    };
    HRESULT result;
    if (FAILED(result = add(1, japanese ? TF_LBMENUF_CHECKED : 0, L"ひらがな"))) return result;
    if (FAILED(result = add(3, TF_LBMENUF_GRAYED, L"（未実装）全角カタカナ"))) return result;
    if (FAILED(result = add(4, TF_LBMENUF_GRAYED, L"（未実装）全角英数字"))) return result;
    if (FAILED(result = add(5, TF_LBMENUF_GRAYED, L"（未実装）半角カタカナ"))) return result;
    if (FAILED(result = add(2, japanese ? 0 : TF_LBMENUF_CHECKED, L"半角英数字"))) return result;
    if (FAILED(result = add(0, TF_LBMENUF_SEPARATOR, L""))) return result;
    if (FAILED(result = add(6, TF_LBMENUF_GRAYED, L"（未実装）単語の追加"))) return result;
    if (FAILED(result = add(7, TF_LBMENUF_GRAYED, L"（未実装）プライベートモード（オフ）"))) return result;
    if (FAILED(result = add(0, TF_LBMENUF_SEPARATOR, L""))) return result;
    return add(8, 0, L"設定");
}
STDMETHODIMP ModeButton::OnMenuSelect(UINT id) {
    if (service_ && (id == 1 || id == 2)) { service_->set_input_mode(id == 1); }
    if (id == 8) {
        // Resolve beside this loaded DLL, so an old in-memory IME cannot launch
        // another deployment's settings and misleadingly report its version.
        HMODULE module{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&kInputModeItem), &module)) return HRESULT_FROM_WIN32(GetLastError());
        std::wstring path(32768, L'\0');
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (!length || length >= path.size()) return E_FAIL;
        path.resize(length);
        const auto directory = std::filesystem::path(path).parent_path() / L"settings";
        const auto exe = directory / L"LiveImeSettings.exe";
        std::wstring command = L"\"" + exe.wstring() + L"\"";
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
            directory.c_str(), &startup, &process)) return HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    }
    return S_OK;
}
STDMETHODIMP ModeButton::GetIcon(HICON* icon) {
    if (!icon) { return E_POINTER; }
    *icon = nullptr;
    const auto size = GetSystemMetrics(SM_CXSMICON);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    const auto dc = CreateCompatibleDC(nullptr);
    if (!dc) { return E_FAIL; }
    const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) { DeleteDC(dc); return E_FAIL; }
    std::memset(pixels, 0, static_cast<std::size_t>(size * size) * 4);
    const auto previous_bitmap = SelectObject(dc, bitmap);
    const auto font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH, L"Yu Gothic UI");
    const auto previous_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
    RECT area{0, 0, size, size};
    DrawTextW(dc, service_ && service_->japanese_mode() ? L"あ" : L"A", 1, &area,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DWORD light_theme = 1; DWORD bytes = sizeof(light_theme);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light_theme, &bytes);
    auto* rgba = static_cast<DWORD*>(pixels);
    for (int i = 0; i < size * size; ++i) {
        const auto alpha = std::max({rgba[i] & 0xff, (rgba[i] >> 8) & 0xff, (rgba[i] >> 16) & 0xff});
        rgba[i] = (alpha << 24) | (light_theme ? 0 : (alpha | alpha << 8 | alpha << 16));
    }
    SelectObject(dc, previous_font); SelectObject(dc, previous_bitmap);
    DeleteObject(font); DeleteDC(dc);
    const auto mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO icon_info{};
    icon_info.fIcon = TRUE; icon_info.hbmColor = bitmap; icon_info.hbmMask = mask;
    *icon = CreateIconIndirect(&icon_info);
    DeleteObject(mask); DeleteObject(bitmap);
    return *icon ? S_OK : E_FAIL;
}
STDMETHODIMP ModeButton::GetText(BSTR* text) {
    if (!text) { return E_POINTER; }
    *text = SysAllocString(service_ && service_->japanese_mode() ? L"あ" : L"A");
    return *text ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP ModeButton::AdviseSink(REFIID iid, IUnknown* object, DWORD* cookie) {
    if (!object || !cookie) { return E_POINTER; }
    if (iid != IID_ITfLangBarItemSink) { return CONNECT_E_CANNOTCONNECT; }
    if (sink_) { return CONNECT_E_ADVISELIMIT; }
    const auto result = object->QueryInterface(IID_ITfLangBarItemSink, reinterpret_cast<void**>(&sink_));
    if (SUCCEEDED(result)) { *cookie = 1; }
    return result;
}
STDMETHODIMP ModeButton::UnadviseSink(DWORD cookie) {
    if (cookie != 1 || !sink_) { return CONNECT_E_NOCONNECTION; }
    sink_->Release(); sink_ = nullptr; return S_OK;
}
}  // namespace windows_live_ime::ime
