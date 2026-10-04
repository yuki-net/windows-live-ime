#include "windows_live_ime/ime/com_module.hpp"

#include <msctf.h>
#include <oleauto.h>

#include <array>
#include <string>
#include <vector>

namespace windows_live_ime::ime {
namespace {

std::wstring clsid_string(REFGUID guid) {
    std::array<wchar_t, 40> buffer{};
    const auto result = StringFromGUID2(guid, buffer.data(), static_cast<int>(buffer.size()));
    return result == 0 ? std::wstring{} : std::wstring(buffer.data());
}

HRESULT write_registry_string(HKEY key, const wchar_t* name, const std::wstring& value) {
    const auto bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const auto status = RegSetValueExW(
        key,
        name,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        bytes);
    return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}

HRESULT register_com_class(const std::wstring& module_path) {
    const auto clsid = clsid_string(CLSID_WindowsLiveImeTextService);
    if (clsid.empty()) {
        return E_FAIL;
    }

    const auto key_path = L"CLSID\\" + clsid;
    HKEY class_key = nullptr;
    auto status = RegCreateKeyExW(
        HKEY_CLASSES_ROOT,
        key_path.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &class_key,
        nullptr);
    if (status != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(status);
    }

    auto result = write_registry_string(class_key, nullptr, L"Live IME Text Service");
    HKEY inproc_key = nullptr;
    if (SUCCEEDED(result)) {
        status = RegCreateKeyExW(
            class_key,
            L"InprocServer32",
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &inproc_key,
            nullptr);
        if (status != ERROR_SUCCESS) {
            result = HRESULT_FROM_WIN32(status);
        }
    }
    if (SUCCEEDED(result)) {
        result = write_registry_string(inproc_key, nullptr, module_path);
    }
    if (SUCCEEDED(result)) {
        result = write_registry_string(inproc_key, L"ThreadingModel", L"Apartment");
    }

    if (inproc_key != nullptr) {
        RegCloseKey(inproc_key);
    }
    RegCloseKey(class_key);
    if (FAILED(result)) {
        RegDeleteTreeW(HKEY_CLASSES_ROOT, key_path.c_str());
    }
    return result;
}

HRESULT get_module_path(std::wstring& path) {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const auto length = GetModuleFileNameW(module_instance(), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        if (length < buffer.size() - 1) {
            path.assign(buffer.data(), length);
            return S_OK;
        }
        if (buffer.size() >= 32768) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }
        buffer.resize(buffer.size() * 2);
    }
}

HRESULT with_com_initialized(bool& should_uninitialize) {
    const auto result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (result == S_OK || result == S_FALSE) {
        should_uninitialize = true;
        return S_OK;
    }
    if (result == RPC_E_CHANGED_MODE) {
        should_uninitialize = false;
        return S_OK;
    }
    return result;
}

HRESULT register_tsf_profile() {
    bool should_uninitialize = false;
    auto result = with_com_initialized(should_uninitialize);
    if (FAILED(result)) {
        return result;
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    result = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles,
        reinterpret_cast<void**>(&profiles));
    if (SUCCEEDED(result)) {
        result = profiles->Register(CLSID_WindowsLiveImeTextService);
    }
    if (SUCCEEDED(result)) {
        std::wstring module_path;
        result = get_module_path(module_path);
        if (SUCCEEDED(result)) {
            constexpr wchar_t description[] = L"Live IME";
            result = profiles->AddLanguageProfile(
                CLSID_WindowsLiveImeTextService,
                MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
                GUID_WindowsLiveImeLanguageProfile,
                description,
                static_cast<ULONG>(sizeof(description) / sizeof(description[0]) - 1),
                module_path.c_str(),
                static_cast<ULONG>(module_path.size()),
                0);
        }
    }
    if (profiles != nullptr) {
        profiles->Release();
    }

    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(result)) {
        result = CoCreateInstance(
            CLSID_TF_CategoryMgr,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_ITfCategoryMgr,
            reinterpret_cast<void**>(&categories));
        if (SUCCEEDED(result)) {
            result = categories->RegisterCategory(
                CLSID_WindowsLiveImeTextService,
                GUID_TFCAT_TIP_KEYBOARD,
                CLSID_WindowsLiveImeTextService);
        }
    }
    if (categories != nullptr) {
        categories->Release();
    }

    if (FAILED(result)) {
        ITfInputProcessorProfiles* rollback = nullptr;
        if (SUCCEEDED(CoCreateInstance(
                CLSID_TF_InputProcessorProfiles,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_ITfInputProcessorProfiles,
                reinterpret_cast<void**>(&rollback)))) {
            rollback->Unregister(CLSID_WindowsLiveImeTextService);
            rollback->Release();
        }
    }
    if (should_uninitialize) {
        CoUninitialize();
    }
    return result;
}

void unregister_tsf_profile() noexcept {
    bool should_uninitialize = false;
    if (FAILED(with_com_initialized(should_uninitialize))) {
        return;
    }

    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(
            CLSID_TF_CategoryMgr,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_ITfCategoryMgr,
            reinterpret_cast<void**>(&categories)))) {
        categories->UnregisterCategory(
            CLSID_WindowsLiveImeTextService,
            GUID_TFCAT_TIP_KEYBOARD,
            CLSID_WindowsLiveImeTextService);
        categories->Release();
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(
            CLSID_TF_InputProcessorProfiles,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_ITfInputProcessorProfiles,
            reinterpret_cast<void**>(&profiles)))) {
        profiles->Unregister(CLSID_WindowsLiveImeTextService);
        profiles->Release();
    }
    if (should_uninitialize) {
        CoUninitialize();
    }
}

}  // namespace
}  // namespace windows_live_ime::ime

extern "C" HRESULT __stdcall DllRegisterServer() {
    using namespace windows_live_ime::ime;
    std::wstring module_path;
    auto result = get_module_path(module_path);
    if (FAILED(result)) {
        return result;
    }

    result = register_com_class(module_path);
    if (FAILED(result)) {
        return result;
    }

    result = register_tsf_profile();
    if (FAILED(result)) {
        const auto clsid = clsid_string(CLSID_WindowsLiveImeTextService);
        if (!clsid.empty()) {
            const auto key_path = L"CLSID\\" + clsid;
            RegDeleteTreeW(HKEY_CLASSES_ROOT, key_path.c_str());
        }
    }
    return result;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    using namespace windows_live_ime::ime;
    unregister_tsf_profile();

    const auto clsid = clsid_string(CLSID_WindowsLiveImeTextService);
    if (clsid.empty()) {
        return E_FAIL;
    }
    const auto key_path = L"CLSID\\" + clsid;
    const auto status = RegDeleteTreeW(HKEY_CLASSES_ROOT, key_path.c_str());
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND
        ? S_OK
        : HRESULT_FROM_WIN32(status);
}
