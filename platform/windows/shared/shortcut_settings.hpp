#pragma once
#include <windows.h>
#include <array>

namespace windows_live_ime::settings {
enum class Action : DWORD { Default, None, Off, On, Toggle, AlternateSpace };
struct Binding { const wchar_t* value; UINT key; UINT modifiers; Action fallback; };
inline constexpr UINT Control = 1, Shift = 2, Alt = 4;
inline constexpr std::array bindings{
    Binding{L"HalfFull", VK_KANJI, 0, Action::Toggle},
    Binding{L"NonConvert", VK_NONCONVERT, 0, Action::Off},
    Binding{L"Convert", VK_CONVERT, 0, Action::On},
    Binding{L"Eisu", VK_CAPITAL, 0, Action::Toggle},
    Binding{L"CtrlSpace", VK_SPACE, Control, Action::None},
    Binding{L"ShiftSpace", VK_SPACE, Shift, Action::AlternateSpace}
};
inline DWORD read(const wchar_t* name, DWORD fallback) {
    DWORD value = fallback, bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\LiveIME\\Shortcuts", name,
        RRF_RT_REG_DWORD, nullptr, &value, &bytes) != ERROR_SUCCESS) return fallback;
    return value;
}
inline Action action(std::size_t index) {
    const auto& binding = bindings[index];
    if (!read(L"Enabled", 1)) return binding.fallback;
    const auto value = read(binding.value, static_cast<DWORD>(Action::Default));
    return value == 0 || value > static_cast<DWORD>(Action::AlternateSpace)
        ? binding.fallback : static_cast<Action>(value);
}
inline void write(const wchar_t* name, DWORD value) {
    HKEY key{};
    auto error = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LiveIME\\Shortcuts", 0,
        nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (error != ERROR_SUCCESS) throw HRESULT_FROM_WIN32(error);
    error = RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    if (error != ERROR_SUCCESS) throw HRESULT_FROM_WIN32(error);
}
// Also support the physical half/full key when a JIS keyboard is exposed as US
// by the VM. This alias can be disabled to type a literal backtick normally.
inline int binding_index(UINT key, UINT modifiers, UINT scan = 0) {
    if (key == VK_OEM_3 && (modifiers == Alt ||
        (modifiers == 0 && scan == 0x29 && read(L"PhysicalHalfFull", 1)))) return 0;
    for (std::size_t i = 0; i < bindings.size(); ++i)
        if (key == bindings[i].key && modifiers == bindings[i].modifiers) return static_cast<int>(i);
    return -1;
}
}
