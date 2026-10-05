#pragma once

#include <Unknwn.h>

namespace windows_live_ime::ime {

class ClassFactory final : public IClassFactory {
public:
    ClassFactory() noexcept;

    STDMETHODIMP QueryInterface(REFIID interface_id, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID interface_id, void** object) override;
    STDMETHODIMP LockServer(BOOL lock) override;

private:
    ~ClassFactory() = default;

    LONG reference_count_{1};
};

}  // namespace windows_live_ime::ime
