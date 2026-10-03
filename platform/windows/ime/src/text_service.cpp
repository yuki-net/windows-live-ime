#include "windows_live_ime/ime/text_service.hpp"

#include "windows_live_ime/ime/com_module.hpp"

#include <Unknwn.h>

namespace windows_live_ime::ime {

TextService::TextService() noexcept {
    module_object_created();
}

TextService::~TextService() {
    if (thread_manager_ != nullptr) {
        thread_manager_->Release();
    }
    module_object_destroyed();
}

STDMETHODIMP TextService::QueryInterface(REFIID interface_id, void** object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;

    if (IsEqualIID(interface_id, IID_IUnknown) || IsEqualIID(interface_id, IID_ITfTextInputProcessor)) {
        *object = static_cast<ITfTextInputProcessor*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TextService::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
}

STDMETHODIMP_(ULONG) TextService::Release() {
    const auto remaining = InterlockedDecrement(&reference_count_);
    if (remaining == 0) {
        delete this;
    }
    return static_cast<ULONG>(remaining);
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* thread_manager, TfClientId client_id) {
    if (thread_manager == nullptr) {
        return E_INVALIDARG;
    }
    if (thread_manager_ != nullptr) {
        return E_UNEXPECTED;
    }

    thread_manager->AddRef();
    thread_manager_ = thread_manager;
    client_id_ = client_id;
    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    if (thread_manager_ != nullptr) {
        thread_manager_->Release();
        thread_manager_ = nullptr;
    }
    client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}

}  // namespace windows_live_ime::ime
