#pragma once

#include <ctfutb.h>
#include <msctf.h>

namespace windows_live_ime::ime {
class TextService;

class ModeButton final : public ITfLangBarItemButton, public ITfSource {
public:
    explicit ModeButton(TextService* service) noexcept;
    void detach() noexcept;
    void refresh();
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override;
    STDMETHODIMP GetStatus(DWORD* status) override;
    STDMETHODIMP Show(BOOL show) override;
    STDMETHODIMP GetTooltipString(BSTR* text) override;
    STDMETHODIMP OnClick(TfLBIClick click, POINT point, const RECT* area) override;
    STDMETHODIMP InitMenu(ITfMenu* menu) override;
    STDMETHODIMP OnMenuSelect(UINT id) override;
    STDMETHODIMP GetIcon(HICON* icon) override;
    STDMETHODIMP GetText(BSTR* text) override;
    STDMETHODIMP AdviseSink(REFIID iid, IUnknown* object, DWORD* cookie) override;
    STDMETHODIMP UnadviseSink(DWORD cookie) override;

private:
    ~ModeButton();
    LONG references_{1};
    TextService* service_;
    ITfLangBarItemSink* sink_{nullptr};
    bool hidden_{false};
};
}
