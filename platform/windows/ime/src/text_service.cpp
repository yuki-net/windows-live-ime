#include "windows_live_ime/ime/text_service.hpp"
#include "windows_live_ime/ime/async_converter.hpp"
#include "windows_live_ime/ime/com_module.hpp"
#include "windows_live_ime/ime/mode_button.hpp"
#include "windows_live_ime/renderer/candidate_window.hpp"
#include <ctfutb.h>
#include <textstor.h>
#include <oleauto.h>
#include <algorithm>
#include <new>

namespace windows_live_ime::ime {
using Microsoft::WRL::ComPtr;
namespace {
std::wstring wide(std::u32string_view text) {
    std::wstring result;
    for (auto c : text) {
        if (c <= 0xffff) result.push_back(static_cast<wchar_t>(c));
        else { c -= 0x10000; result.push_back(static_cast<wchar_t>(0xd800 + (c >> 10))); result.push_back(static_cast<wchar_t>(0xdc00 + (c & 0x3ff))); }
    }
    return result;
}
void unadvise(IUnknown* object, DWORD cookie) {
    ComPtr<ITfSource> source;
    if (object && cookie != TF_INVALID_COOKIE && SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&source)))) source->UnadviseSink(cookie);
}
void set_value(ITfCompartment* compartment, TfClientId client, LONG value) {
    if (!compartment) return;
    VARIANT data{}; data.vt = VT_I4; data.lVal = value; compartment->SetValue(client, &data);
}
LONG get_value(ITfCompartment* compartment, LONG fallback) {
    VARIANT data{};
    if (!compartment || FAILED(compartment->GetValue(&data))) return fallback;
    const auto value = data.vt == VT_I4 ? data.lVal : fallback;
    VariantClear(&data); return value;
}
}
class CompositionEditSession final : public ITfEditSession {
public:
    CompositionEditSession(TextService* owner, ITfContext* context, detail::EditOperation operation,
        std::wstring text, std::uint64_t generation, std::size_t cursor)
        : owner_(owner), context_(context), operation_(operation), text_(std::move(text)), generation_(generation), cursor_(cursor) { owner_->AddRef(); module_object_created(); }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER; *object = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfEditSession) return E_NOINTERFACE;
        *object = static_cast<ITfEditSession*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references_)); }
    STDMETHODIMP_(ULONG) Release() override { const auto count = InterlockedDecrement(&references_); if (!count) delete this; return static_cast<ULONG>(count); }
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        try { return owner_->edit(cookie, context_.Get(), operation_, text_, generation_, cursor_); } catch (...) { return E_FAIL; }
    }
private:
    ~CompositionEditSession() { owner_->Release(); module_object_destroyed(); }
    LONG references_{1}; TextService* owner_; ComPtr<ITfContext> context_;
    detail::EditOperation operation_; std::wstring text_; std::uint64_t generation_; std::size_t cursor_;
};
TextService::TextService() noexcept { module_object_created(); }
TextService::~TextService() { converter_.reset(); candidate_ui_.reset(); if (message_window_) DestroyWindow(message_window_); if (thread_manager_) thread_manager_->Release(); module_object_destroyed(); }
STDMETHODIMP TextService::QueryInterface(REFIID iid, void** object) {
    if (!object) return E_POINTER; *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor || iid == IID_ITfTextInputProcessorEx) *object = static_cast<ITfTextInputProcessorEx*>(this);
    else if (iid == IID_ITfKeyEventSink) *object = static_cast<ITfKeyEventSink*>(this);
    else if (iid == IID_ITfCompositionSink) *object = static_cast<ITfCompositionSink*>(this);
    else if (iid == IID_ITfThreadMgrEventSink) *object = static_cast<ITfThreadMgrEventSink*>(this);
    else if (iid == IID_ITfCompartmentEventSink) *object = static_cast<ITfCompartmentEventSink*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
STDMETHODIMP_(ULONG) TextService::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&reference_count_)); }
STDMETHODIMP_(ULONG) TextService::Release() { const auto count = InterlockedDecrement(&reference_count_); if (!count) delete this; return static_cast<ULONG>(count); }
STDMETHODIMP TextService::Activate(ITfThreadMgr* manager, TfClientId client) { return ActivateEx(manager, client, 0); }
STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* manager, TfClientId client, DWORD) {
    if (!manager) return E_INVALIDARG; if (thread_manager_) return E_UNEXPECTED;
    manager->AddRef(); thread_manager_ = manager; client_id_ = client;
    try {
        WNDCLASSW wc{}; wc.lpfnWndProc = window_proc; wc.hInstance = module_instance(); wc.lpszClassName = L"LiveImeConversionMessages";
        RegisterClassW(&wc);
        message_window_ = CreateWindowExW(0, wc.lpszClassName, L"Live IME", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, this);
        if (!message_window_) { Deactivate(); return E_FAIL; }
        converter_ = std::make_unique<AsyncConverter>(message_window_);
        candidate_ui_ = std::make_unique<renderer::CandidateWindow>(module_instance());
        candidate_ui_->set_selection_handler([this](std::size_t index) { choose_candidate(index, true); });
        auto result = manager->QueryInterface(IID_PPV_ARGS(&keystroke_manager_));
        if (SUCCEEDED(result)) result = keystroke_manager_->AdviseKeyEventSink(client, this, TRUE);
        if (FAILED(result)) { Deactivate(); return result; }
        keys_advised_ = true;
        ComPtr<ITfSource> source;
        if (SUCCEEDED(manager->QueryInterface(IID_PPV_ARGS(&source)))) source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &thread_sink_cookie_);
        ComPtr<ITfCompartmentMgr> compartments;
        if (SUCCEEDED(manager->QueryInterface(IID_PPV_ARGS(&compartments)))) {
            compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &open_compartment_);
            compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &conversion_compartment_);
            source.Reset(); if (open_compartment_ && SUCCEEDED(open_compartment_.As(&source))) source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &open_sink_cookie_);
            source.Reset(); if (conversion_compartment_ && SUCCEEDED(conversion_compartment_.As(&source))) source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &conversion_sink_cookie_);
        }
        ComPtr<ITfLangBarItemMgr> bar;
        if (SUCCEEDED(manager->QueryInterface(IID_PPV_ARGS(&bar)))) { mode_button_ = new ModeButton(this); bar->AddItem(mode_button_); }
        DWORD saved = 1, bytes = sizeof(saved);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\LiveIME", L"JapaneseMode", RRF_RT_REG_DWORD, nullptr, &saved, &bytes);
        set_input_mode(saved != 0);
        return S_OK;
    } catch (...) { Deactivate(); return E_OUTOFMEMORY; }
}
STDMETHODIMP TextService::Deactivate() {
    finish_input(); converter_.reset();
    if (keys_advised_) keystroke_manager_->UnadviseKeyEventSink(client_id_);
    keys_advised_ = false; keystroke_manager_.Reset();
    unadvise(thread_manager_, thread_sink_cookie_); unadvise(open_compartment_.Get(), open_sink_cookie_); unadvise(conversion_compartment_.Get(), conversion_sink_cookie_);
    thread_sink_cookie_ = open_sink_cookie_ = conversion_sink_cookie_ = TF_INVALID_COOKIE;
    if (mode_button_) { ComPtr<ITfLangBarItemMgr> bar; if (thread_manager_ && SUCCEEDED(thread_manager_->QueryInterface(IID_PPV_ARGS(&bar)))) bar->RemoveItem(mode_button_); mode_button_->detach(); mode_button_->Release(); mode_button_ = nullptr; }
    open_compartment_.Reset(); conversion_compartment_.Reset(); composition_.Reset(); composition_context_.Reset(); clear_input(); candidate_ui_.reset();
    if (message_window_) DestroyWindow(message_window_); message_window_ = nullptr;
    if (thread_manager_) thread_manager_->Release(); thread_manager_ = nullptr; client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}
void TextService::set_input_mode(bool japanese) {
    if (japanese_mode_ != japanese) finish_input(); japanese_mode_ = japanese;
    updating_mode_ = true;
    set_value(open_compartment_.Get(), client_id_, japanese ? 1 : 0);
    set_value(conversion_compartment_.Get(), client_id_, japanese ? TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_FULLSHAPE : 0);
    updating_mode_ = false;
    HKEY key{}; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LiveIME", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        DWORD saved = japanese ? 1 : 0; RegSetValueExW(key, L"JapaneseMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&saved), sizeof(saved)); RegCloseKey(key);
    }
    if (mode_button_) mode_button_->refresh();
}
void TextService::toggle_input_mode() { set_input_mode(!japanese_mode_); }
STDMETHODIMP TextService::OnChange(REFGUID) { if (!updating_mode_) set_input_mode(get_value(open_compartment_.Get(), 1) != 0 && (get_value(conversion_compartment_.Get(), TF_CONVERSIONMODE_NATIVE) & TF_CONVERSIONMODE_NATIVE) != 0); return S_OK; }
bool TextService::context_accepts_input(ITfContext* context) const {
    if (!context) return false;
    TF_STATUS status{}; if (FAILED(context->GetStatus(&status)) || (status.dwDynamicFlags & TS_SD_READONLY)) return false;
    ComPtr<ITfCompartmentMgr> mgr; if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&mgr)))) {
        for (const auto guid : {GUID_COMPARTMENT_KEYBOARD_DISABLED, GUID_COMPARTMENT_EMPTYCONTEXT}) {
            ComPtr<ITfCompartment> compartment; if (SUCCEEDED(mgr->GetCompartment(guid, &compartment)) && get_value(compartment.Get(), 0)) return false;
        }
    }
    return true;
}
std::u32string TextService::key_text(WPARAM key, LPARAM flags) const {
    BYTE state[256]{}; if (!GetKeyboardState(state)) return {};
    wchar_t text[8]{}; const auto count = ToUnicodeEx(static_cast<UINT>(key), static_cast<UINT>((flags >> 16) & 0xff), state, text, 8, 4, GetKeyboardLayout(0));
    std::u32string result;
    for (int i = 0; i < count; ++i) { char32_t c = text[i]; if (c >= 0xd800 && c <= 0xdbff && i + 1 < count) c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00); if (c >= 0x20 && c != 0x7f) result.push_back(c); }
    return result;
}
bool TextService::wants_key(ITfContext* context, WPARAM key, LPARAM flags) const {
    if (!context_accepts_input(context)) return false;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0, alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (key == VK_KANJI || key == VK_NONCONVERT || key == VK_CONVERT || (ctrl && !alt && key == VK_SPACE)) return true;
    if (!japanese_mode_ || ctrl || alt) return false;
    if (!input_.empty() && (key == VK_RETURN || key == VK_ESCAPE || key == VK_BACK || key == VK_DELETE || key == VK_LEFT || key == VK_RIGHT || key == VK_HOME || key == VK_END || key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT || (key >= VK_F6 && key <= VK_F10))) return true;
    return !key_text(key, flags).empty();
}
STDMETHODIMP TextService::OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) { if (!eaten) return E_POINTER; try { *eaten = wants_key(context, key, flags); } catch (...) { *eaten = FALSE; } return S_OK; }
STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
STDMETHODIMP TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) {
    if (!eaten) return E_POINTER; *eaten = FALSE;
    try {
        if (!wants_key(context, key, flags)) { if (!input_.empty()) finish_input(); return S_OK; }
        *eaten = TRUE;
        if (key == VK_KANJI || (key == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000))) { toggle_input_mode(); return S_OK; }
        if (key == VK_NONCONVERT) { set_input_mode(false); return S_OK; }
        if (key == VK_CONVERT && !japanese_mode_) { set_input_mode(true); return S_OK; }
        if (input_context_ && input_context_.Get() != context) finish_input();
        input_context_ = context;
        if (!input_.empty()) {
            if (key == VK_RETURN) { finish_input(); return S_OK; }
            if (key == VK_ESCAPE) { if (selecting_ || literal_) { refresh_input(context, false); } else finish_input(true); return S_OK; }
            if (key == VK_SPACE || key == VK_CONVERT || key == VK_DOWN || key == VK_UP || key == VK_PRIOR || key == VK_NEXT) {
                if (candidates_.empty()) candidates_.push_back({input_.reading().kana, {}});
                if (selecting_) { const auto step = key == VK_PRIOR || key == VK_NEXT ? 9 : 1; if (key == VK_UP || key == VK_PRIOR) selected_ = (selected_ + candidates_.size() - static_cast<std::size_t>(step) % candidates_.size()) % candidates_.size(); else selected_ = (selected_ + step) % candidates_.size(); }
                selecting_ = true; literal_ = false; choose_candidate(selected_, false); return S_OK;
            }
            if (selecting_ && key >= '1' && key <= '9') { const auto index = (selected_ / 9) * 9 + key - '1'; if (index < candidates_.size()) choose_candidate(index, true); return S_OK; }
            if (key >= VK_F6 && key <= VK_F10) {
                (void)coordinator_.invalidate(); selecting_ = false; literal_ = true;
                const auto reading = input_.reading(true).text();
                display_text_ = key == VK_F6 ? reading : key == VK_F7 ? core::to_katakana(reading) : key == VK_F8 ? core::to_katakana(reading, true) : key == VK_F9 ? core::to_full_width_ascii(input_.raw()) : input_.raw();
                display_cursor_ = display_text_.size(); request_edit(context, detail::EditOperation::Update, true); return S_OK;
            }
            if (key == VK_BACK || key == VK_DELETE) { if (key == VK_BACK) (void)input_.erase_previous(); else (void)input_.erase_next(); refresh_input(context); return S_OK; }
            if (key == VK_LEFT || key == VK_RIGHT || key == VK_HOME || key == VK_END) {
                if (key == VK_LEFT) input_.move_left(); else if (key == VK_RIGHT) input_.move_right(); else if (key == VK_HOME) while (input_.cursor()) input_.move_left(); else while (input_.cursor() < input_.raw().size()) input_.move_right();
                refresh_input(context, false); return S_OK;
            }
        }
        const auto text = key_text(key, flags); if (text.empty()) { *eaten = FALSE; return S_OK; }
        if (selecting_) { finish_input(); input_context_ = context; }
        if (input_.empty() && text == U" ") { *eaten = FALSE; input_context_.Reset(); return S_OK; }
        input_.insert(text); refresh_input(context); return S_OK;
    } catch (...) { *eaten = FALSE; return E_FAIL; }
}
STDMETHODIMP TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
void TextService::refresh_input(ITfContext* context, bool convert) {
    selecting_ = false; literal_ = !convert; candidates_.clear(); selected_ = 0;
    if (input_.empty()) { finish_input(true); return; }
    const auto reading = input_.reading(); display_text_ = reading.text(); display_cursor_ = input_.display_cursor();
    if (candidate_ui_) candidate_ui_->hide();
    if (convert && !reading.kana.empty()) converter_->submit(coordinator_.make_request(reading.kana)); else (void)coordinator_.invalidate();
    request_edit(context, detail::EditOperation::Update, true);
}
void TextService::conversion_ready() {
    const auto response = converter_->take_result();
    if (!response || !coordinator_.is_current(*response) || input_.empty() || !input_context_ || literal_ || selecting_) return;
    candidates_ = response->candidates; engine_available_ = !candidates_.empty();
    const auto reading = input_.reading();
    for (const auto& fallback : {reading.kana, core::to_katakana(reading.kana)}) if (!fallback.empty() && std::none_of(candidates_.begin(), candidates_.end(), [&](const auto& c) { return c.text == fallback; })) candidates_.push_back({fallback, {}});
    if (candidates_.empty()) return;
    selected_ = 0; display_text_ = candidates_[0].text + reading.pending; display_cursor_ = display_text_.size();
    request_edit(input_context_.Get(), detail::EditOperation::Update, false);
}
void TextService::choose_candidate(std::size_t index, bool commit) {
    if (index >= candidates_.size()) return;
    selected_ = index; display_text_ = candidates_[index].text + input_.reading().pending; display_cursor_ = display_text_.size();
    if (commit) finish_input(); else request_edit(input_context_.Get(), detail::EditOperation::Update, true);
}
void TextService::clear_input() { (void)coordinator_.invalidate(); input_.clear(); display_text_.clear(); candidates_.clear(); selecting_ = literal_ = false; selected_ = display_cursor_ = 0; input_context_.Reset(); if (candidate_ui_) candidate_ui_->hide(); }
void TextService::finish_input(bool cancel) {
    ComPtr<ITfContext> context = input_context_ ? input_context_ : composition_context_;
    if (context && (composition_ || !input_.empty())) {
        if (!cancel && !literal_ && !input_.reading().pending.empty()) {
            const auto reading = input_.reading(); const auto finalized = input_.reading(true);
            if (!candidates_.empty() && selected_ < candidates_.size() && finalized.kana.starts_with(reading.kana)) display_text_ = candidates_[selected_].text + finalized.kana.substr(reading.kana.size()) + finalized.pending;
            else display_text_ = finalized.text();
        }
        request_edit(context.Get(), cancel ? detail::EditOperation::Cancel : detail::EditOperation::Commit, true);
    }
    clear_input();
}
HRESULT TextService::request_edit(ITfContext* context, detail::EditOperation operation, bool synchronous) {
    if (!context || client_id_ == TF_CLIENTID_NULL) return E_UNEXPECTED;
    const auto text = wide(display_text_); const auto cursor = wide(std::u32string_view(display_text_).substr(0, display_cursor_)).size();
    auto* session = new (std::nothrow) CompositionEditSession(this, context, operation, text, ++edit_generation_, cursor);
    if (!session) return E_OUTOFMEMORY;
    HRESULT result_session = E_FAIL;
    auto result = context->RequestEditSession(client_id_, session, TF_ES_READWRITE | (synchronous ? TF_ES_SYNC : TF_ES_ASYNC), &result_session);
    if (synchronous && (result == TF_E_SYNCHRONOUS || result_session == TF_E_SYNCHRONOUS || result_session == TS_E_NOLOCK)) result = context->RequestEditSession(client_id_, session, TF_ES_READWRITE | TF_ES_ASYNC, &result_session);
    session->Release(); return FAILED(result) ? result : result_session;
}
HRESULT TextService::edit(TfEditCookie cookie, ITfContext* context, detail::EditOperation operation, const std::wstring& text, std::uint64_t generation, std::size_t cursor) {
    if (!thread_manager_) return S_FALSE;
    if (operation == detail::EditOperation::Update && generation != edit_generation_) return S_FALSE;
    if (composition_ && composition_context_.Get() != context) return S_FALSE;
    if (!composition_) {
        // A commit can arrive before the first asynchronous update has run.
        // Insert its snapshot instead of dropping the user's pending text.
        if (operation == detail::EditOperation::Cancel || text.empty()) return S_OK;
        ComPtr<ITfInsertAtSelection> insert; auto result = context->QueryInterface(IID_PPV_ARGS(&insert)); if (FAILED(result)) return result;
        ComPtr<ITfRange> range; result = insert->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, L"", 0, &range); if (FAILED(result)) return result;
        ComPtr<ITfContextComposition> composing; result = context->QueryInterface(IID_PPV_ARGS(&composing)); if (FAILED(result)) return result;
        result = composing->StartComposition(cookie, range.Get(), this, &composition_); if (FAILED(result)) return result;
        composition_context_ = context;
    }
    ComPtr<ITfRange> range; auto result = composition_->GetRange(&range); if (FAILED(result)) return result;
    result = range->SetText(cookie, 0, operation == detail::EditOperation::Cancel ? L"" : text.c_str(), operation == detail::EditOperation::Cancel ? 0 : static_cast<LONG>(text.size())); if (FAILED(result)) return result;
    ComPtr<ITfRange> caret; result = range->Clone(&caret); if (FAILED(result)) return result;
    result = caret->Collapse(cookie, TF_ANCHOR_START); if (FAILED(result)) return result;
    LONG shifted{};
    // Extend the end of a collapsed range; moving its start forwards is
    // clamped by its end and would leave the caret before the composition.
    result = caret->ShiftEnd(cookie, static_cast<LONG>(std::min(cursor, text.size())), &shifted, nullptr);
    if (FAILED(result)) return result;
    result = caret->Collapse(cookie, TF_ANCHOR_END); if (FAILED(result)) return result;
    TF_SELECTION selection{caret.Get(), {TF_AE_NONE, FALSE}}; context->SetSelection(cookie, 1, &selection);
    if (operation != detail::EditOperation::Update) { ending_composition_ = true; ComPtr<ITfComposition> ending = composition_; result = ending->EndComposition(cookie); ending_composition_ = false; composition_.Reset(); composition_context_.Reset(); if (candidate_ui_) candidate_ui_->hide(); }
    else update_candidate_window(cookie, context, range.Get());
    return result;
}
void TextService::update_candidate_window(TfEditCookie cookie, ITfContext* context, ITfRange* range) {
    if (!candidate_ui_ || !selecting_) return;
    ComPtr<ITfContextView> view; RECT rect{}; BOOL clipped{};
    if (FAILED(context->GetActiveView(&view)) || FAILED(view->GetTextExt(cookie, range, &rect, &clipped))) return;
    std::vector<std::wstring> candidates; for (const auto& candidate : candidates_) candidates.push_back(wide(candidate.text));
    candidate_ui_->show(candidates, selected_, rect, engine_available_);
}
STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie, ITfComposition* composition) { if (!ending_composition_ && composition_.Get() == composition) { composition_.Reset(); composition_context_.Reset(); clear_input(); } return S_OK; }
STDMETHODIMP TextService::OnSetFocus(BOOL foreground) { if (!foreground) finish_input(); return S_OK; }
STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) { if (focus != previous) finish_input(); return S_OK; }
STDMETHODIMP TextService::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP TextService::OnPopContext(ITfContext* context) { if (input_context_.Get() == context) finish_input(); return S_OK; }
LRESULT CALLBACK TextService::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* service = reinterpret_cast<TextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) { service = static_cast<TextService*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams); SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(service)); }
    if (message == kConversionReadyMessage && service) { service->AddRef(); try { if (service->converter_) service->conversion_ready(); } catch (...) {} service->Release(); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace windows_live_ime::ime
