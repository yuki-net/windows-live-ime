#pragma once

#include <msctf.h>

namespace windows_live_ime::ime {

class TextService final : public ITfTextInputProcessor {
public:
    TextService() noexcept;

    STDMETHODIMP QueryInterface(REFIID interface_id, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Activate(ITfThreadMgr* thread_manager, TfClientId client_id) override;
    STDMETHODIMP Deactivate() override;

private:
    ~TextService();

    LONG reference_count_{1};
    ITfThreadMgr* thread_manager_{nullptr};
    TfClientId client_id_{TF_CLIENTID_NULL};
};

}  // namespace windows_live_ime::ime
