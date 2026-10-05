#include "windows_live_ime/ime/class_factory.hpp"

#include "windows_live_ime/ime/com_module.hpp"
#include "windows_live_ime/ime/text_service.hpp"

#include <new>

namespace windows_live_ime::ime {

ClassFactory::ClassFactory() noexcept {
    module_object_created();
}

STDMETHODIMP ClassFactory::QueryInterface(REFIID interface_id, void** object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;

    if (IsEqualIID(interface_id, IID_IUnknown) || IsEqualIID(interface_id, IID_IClassFactory)) {
        *object = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
}

STDMETHODIMP_(ULONG) ClassFactory::Release() {
    const auto remaining = InterlockedDecrement(&reference_count_);
    if (remaining == 0) {
        delete this;
    }
    return static_cast<ULONG>(remaining);
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* outer, REFIID interface_id, void** object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;
    if (outer != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    auto* service = new (std::nothrow) TextService();
    if (service == nullptr) {
        return E_OUTOFMEMORY;
    }
    const auto result = service->QueryInterface(interface_id, object);
    service->Release();
    return result;
}

STDMETHODIMP ClassFactory::LockServer(BOOL lock) {
    if (lock) {
        module_lock();
    } else {
        module_unlock();
    }
    return S_OK;
}

}  // namespace windows_live_ime::ime

extern "C" HRESULT __stdcall DllGetClassObject(
    REFCLSID class_id,
    REFIID interface_id,
    void** object) {
    using namespace windows_live_ime::ime;
    if (!IsEqualCLSID(class_id, CLSID_WindowsLiveImeTextService)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ClassFactory();
    if (factory == nullptr) {
        return E_OUTOFMEMORY;
    }
    const auto result = factory->QueryInterface(interface_id, object);
    factory->Release();
    return result;
}
