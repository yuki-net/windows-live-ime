#pragma once

#include <msctf.h>
#include <wrl/client.h>

#include "windows_live_ime/core/live_conversion_coordinator.hpp"
#include "windows_live_ime/core/romaji_input.hpp"

#include <memory>
#include <string>
#include <vector>

namespace windows_live_ime::renderer { class CandidateWindow; }
namespace windows_live_ime::ime {

class AsyncConverter;
class ModeButton;
class CompositionEditSession;
namespace detail { enum class EditOperation { Update, Commit, Cancel }; }

class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfCompositionSink,
                          public ITfThreadMgrEventSink,
                          public ITfCompartmentEventSink {
public:
    TextService() noexcept;

    STDMETHODIMP QueryInterface(REFIID interface_id, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Activate(ITfThreadMgr* thread_manager, TfClientId client_id) override;
    STDMETHODIMP Deactivate() override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* thread_manager, TfClientId client_id, DWORD flags) override;

    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;
    STDMETHODIMP OnCompositionTerminated(TfEditCookie cookie, ITfComposition* composition) override;
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* document) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* document) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) override;
    STDMETHODIMP OnPushContext(ITfContext* context) override;
    STDMETHODIMP OnPopContext(ITfContext* context) override;
    STDMETHODIMP OnChange(REFGUID compartment) override;

    [[nodiscard]] bool japanese_mode() const noexcept { return japanese_mode_; }
    void set_input_mode(bool japanese);
    void toggle_input_mode();

private:
    ~TextService();
    friend class CompositionEditSession;
    [[nodiscard]] bool wants_key(ITfContext* context, WPARAM key, LPARAM flags) const;
    [[nodiscard]] bool context_accepts_input(ITfContext* context) const;
    [[nodiscard]] std::u32string key_text(WPARAM key, LPARAM flags) const;
    HRESULT request_edit(ITfContext* context, detail::EditOperation operation, bool synchronous);
    HRESULT edit(TfEditCookie cookie, ITfContext* context, detail::EditOperation operation,
                 const std::wstring& text, std::uint64_t generation, std::size_t cursor);
    void refresh_input(ITfContext* context, bool convert = true);
    void conversion_ready();
    void clear_input();
    void finish_input(bool cancel = false);
    void choose_candidate(std::size_t index, bool commit);
    void update_candidate_window(TfEditCookie cookie, ITfContext* context, ITfRange* range);
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    LONG reference_count_{1};
    ITfThreadMgr* thread_manager_{nullptr};
    TfClientId client_id_{TF_CLIENTID_NULL};
    Microsoft::WRL::ComPtr<ITfKeystrokeMgr> keystroke_manager_;
    Microsoft::WRL::ComPtr<ITfComposition> composition_;
    Microsoft::WRL::ComPtr<ITfContext> composition_context_;
    Microsoft::WRL::ComPtr<ITfContext> input_context_;
    Microsoft::WRL::ComPtr<ITfCompartment> open_compartment_;
    Microsoft::WRL::ComPtr<ITfCompartment> conversion_compartment_;
    DWORD thread_sink_cookie_{TF_INVALID_COOKIE};
    DWORD open_sink_cookie_{TF_INVALID_COOKIE};
    DWORD conversion_sink_cookie_{TF_INVALID_COOKIE};
    bool keys_advised_{false};
    bool japanese_mode_{true};
    bool updating_mode_{false};
    bool ending_composition_{false};
    bool selecting_{false};
    bool literal_{false};
    bool engine_available_{true};
    std::uint64_t edit_generation_{0};
    std::size_t selected_{0};
    std::size_t display_cursor_{0};
    core::RomajiInput input_;
    core::LiveConversionCoordinator coordinator_;
    std::vector<core::Candidate> candidates_;
    std::u32string display_text_;
    HWND message_window_{nullptr};
    std::unique_ptr<AsyncConverter> converter_;
    std::unique_ptr<renderer::CandidateWindow> candidate_ui_;
    ModeButton* mode_button_{nullptr};
};

}  // namespace windows_live_ime::ime
