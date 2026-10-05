#include "../../shared/shortcut_settings.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace windows_live_ime::settings;
    HKEY testKey{};
    const auto name=std::wstring(L"Software\\LiveIME-ShortcutTests-")+std::to_wstring(GetCurrentProcessId());
    const auto create=RegCreateKeyExW(HKEY_CURRENT_USER,name.c_str(),0,nullptr,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,nullptr,&testKey,nullptr);
    if (create) { std::cerr << "Registry test key error: " << create << '\n'; return 1; }
    const auto overrideResult=RegOverridePredefKey(HKEY_CURRENT_USER,testKey);
    if (overrideResult) { std::cerr << "Registry override error: " << overrideResult << '\n'; RegCloseKey(testKey); return 1; }
    int result=0;
    try {
        const auto check=[](bool condition) { if (!condition) throw std::runtime_error("Shortcut assertion failed"); };
        check(action(0)==Action::Toggle && action(1)==Action::Off && action(2)==Action::On);
        check(action(3)==Action::Toggle && action(4)==Action::None && action(5)==Action::AlternateSpace);
        check(binding_index(VK_OEM_3,0,0x29)==0 && binding_index(VK_OEM_3,Alt)==0);
        check(binding_index(VK_SPACE,Control|Shift)==-1);
        write(L"PhysicalHalfFull",0); check(binding_index(VK_OEM_3,0,0x29)==-1);
        for (std::size_t i=0;i<bindings.size();++i) {
            check(binding_index(bindings[i].key,bindings[i].modifiers)==static_cast<int>(i));
            for (DWORD value=1;value<=5;++value) { write(bindings[i].value,value); check(action(i)==static_cast<Action>(value)); }
            write(bindings[i].value,99); check(action(i)==bindings[i].fallback);
        }
        write(L"HalfFull",static_cast<DWORD>(Action::Off));
        write(L"Enabled",0); check(action(0)==Action::Toggle);
        std::cout << "Shortcut defaults, persistence, modifiers and US alias passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result=1; }
    catch (HRESULT error) { std::cerr << "Registry write failed: " << error << '\n'; result=1; }
    RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);
    RegCloseKey(testKey);
    RegDeleteTreeW(HKEY_CURRENT_USER,name.c_str());
    return result;
}
