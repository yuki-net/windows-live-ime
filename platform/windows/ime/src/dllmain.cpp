#include "windows_live_ime/ime/com_module.hpp"

#include <atomic>

namespace {

std::atomic<unsigned long> g_live_objects{0};
std::atomic<unsigned long> g_server_locks{0};
HINSTANCE g_instance = nullptr;

}  // namespace

namespace windows_live_ime::ime {

const CLSID CLSID_WindowsLiveImeTextService =
    {0x64fd3b11, 0x84d2, 0x4ad3, {0x9f, 0x4e, 0x24, 0x41, 0x00, 0x20, 0x10, 0x01}};
const GUID GUID_WindowsLiveImeLanguageProfile =
    {0x64fd3b12, 0x84d2, 0x4ad3, {0x9f, 0x4e, 0x24, 0x41, 0x00, 0x20, 0x10, 0x01}};

void module_object_created() noexcept {
    g_live_objects.fetch_add(1, std::memory_order_relaxed);
}

void module_object_destroyed() noexcept {
    g_live_objects.fetch_sub(1, std::memory_order_relaxed);
}

void module_lock() noexcept {
    g_server_locks.fetch_add(1, std::memory_order_relaxed);
}

void module_unlock() noexcept {
    g_server_locks.fetch_sub(1, std::memory_order_relaxed);
}

HINSTANCE module_instance() noexcept {
    return g_instance;
}

}  // namespace windows_live_ime::ime

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    using namespace windows_live_ime::ime;
    return g_live_objects.load(std::memory_order_acquire) == 0 &&
                   g_server_locks.load(std::memory_order_acquire) == 0
        ? S_OK
        : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID class_id, REFIID interface_id, void** object);
extern "C" HRESULT __stdcall DllRegisterServer();
extern "C" HRESULT __stdcall DllUnregisterServer();
