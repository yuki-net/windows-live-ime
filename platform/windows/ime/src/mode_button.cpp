#include "windows_live_ime/ime/mode_button.hpp"
#include "windows_live_ime/ime/text_service.hpp"
#include "windows_live_ime/ime/com_module.hpp"

#include <oleauto.h>
#include <olectl.h>
#include <algorithm>
#include <cstring>

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
        ? L"Live IME：日本語入力（Ctrl+Spaceで英数）" : L"Live IME：英数入力（Ctrl+Spaceで日本語）");
    return *text ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP ModeButton::OnClick(TfLBIClick click, POINT, const RECT*) {
    if (service_ && click == TF_LBI_CLK_LEFT) { service_->toggle_input_mode(); }
    return S_OK;
}
STDMETHODIMP ModeButton::InitMenu(ITfMenu* menu) {
    if (!menu) { return E_POINTER; }
    const auto japanese = service_ && service_->japanese_mode();
    menu->AddMenuItem(1, japanese ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr, L"日本語入力", 5, nullptr);
    return menu->AddMenuItem(2, japanese ? 0 : TF_LBMENUF_CHECKED, nullptr, nullptr, L"英数入力", 4, nullptr);
}
STDMETHODIMP ModeButton::OnMenuSelect(UINT id) {
    if (service_ && (id == 1 || id == 2)) { service_->set_input_mode(id == 1); }
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
