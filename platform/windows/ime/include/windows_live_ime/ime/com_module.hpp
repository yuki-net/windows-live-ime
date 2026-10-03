#pragma once

#include <Windows.h>

namespace windows_live_ime::ime {

extern const CLSID CLSID_WindowsLiveImeTextService;
extern const GUID GUID_WindowsLiveImeLanguageProfile;

void module_object_created() noexcept;
void module_object_destroyed() noexcept;
void module_lock() noexcept;
void module_unlock() noexcept;
[[nodiscard]] HINSTANCE module_instance() noexcept;

}  // namespace windows_live_ime::ime
