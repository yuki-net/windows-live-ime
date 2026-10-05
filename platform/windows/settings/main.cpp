#include <windows.h>
#include <tlhelp32.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "../shared/shortcut_settings.hpp"

namespace {
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace fs = std::filesystem;

fs::path executable_path() {
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw hresult_error(HRESULT_FROM_WIN32(GetLastError()));
    path.resize(length); return fs::path(path);
}
struct BuildInfo {
    std::wstring version{L"情報なし"};
    std::wstring commit{L"情報なし"};
    std::wstring built_at{L"情報なし"};
    bool dirty{false};
    bool found{false};
};
BuildInfo read_info(const fs::path& directory) {
    BuildInfo info;
    std::ifstream file(directory / L"build-info.json", std::ios::binary);
    if (!file) return info;
    try {
        std::ostringstream buffer; buffer << file.rdbuf();
        const auto json = Windows::Data::Json::JsonObject::Parse(to_hstring(buffer.str()));
        info.version = json.GetNamedString(L"version").c_str();
        info.commit = json.GetNamedString(L"commit").c_str();
        info.built_at = json.GetNamedString(L"builtAt").c_str();
        info.dirty = json.GetNamedBoolean(L"dirty");
        info.found = true;
    } catch (...) { /* Old or incomplete deployments are shown as unknown. */ }
    return info;
}
fs::path registered_ime() {
    wchar_t buffer[32768]{}; DWORD bytes = sizeof(buffer);
    const auto result = RegGetValueW(HKEY_CLASSES_ROOT,
        L"CLSID\\{64fd3b11-84d2-4ad3-9f4e-244100201001}\\InprocServer32",
        nullptr, RRF_RT_REG_SZ, nullptr, buffer, &bytes);
    return result == ERROR_SUCCESS ? fs::path(buffer) : fs::path{};
}
std::wstring identity(const BuildInfo& info) {
    return info.found ? L"v" + info.version + L"  /  " + info.commit + (info.dirty ? L"（未コミットの変更あり）" : L"")
                      : L"バージョン情報なし（旧版または情報ファイルなし）";
}
std::wstring loaded_ime_status() {
    std::wstring result;
    const auto processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processes == INVALID_HANDLE_VALUE) return L"使用中のIMEを確認できません";
    PROCESSENTRY32W process{}; process.dwSize = sizeof(process);
    if (Process32FirstW(processes, &process)) do {
        if (_wcsicmp(process.szExeFile, L"notepad.exe") && _wcsicmp(process.szExeFile, L"explorer.exe") && _wcsicmp(process.szExeFile, L"ctfmon.exe")) continue;
        const auto modules = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process.th32ProcessID);
        if (modules == INVALID_HANDLE_VALUE) continue;
        MODULEENTRY32W module{}; module.dwSize = sizeof(module);
        if (Module32FirstW(modules, &module)) do {
            if (_wcsicmp(module.szModule, L"windows-live-ime.dll")) continue;
            const fs::path path(module.szExePath);
            if (!result.empty()) result += L"\n";
            result += std::wstring(process.szExeFile) + L"：" + path.parent_path().filename().wstring()
                      + L"  /  " + identity(read_info(path.parent_path()));
        } while (Module32NextW(modules, &module));
        CloseHandle(modules);
    } while (Process32NextW(processes, &process));
    CloseHandle(processes);
    return result.empty() ? L"対象アプリでLive IMEの読み込みを検出していません" : result;
}
struct SettingsApp : ApplicationT<SettingsApp, Markup::IXamlMetadataProvider> {
    Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata_;
    Markup::IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type) { return metadata_.GetXamlType(type); }
    Markup::IXamlType GetXamlType(hstring const& name) { return metadata_.GetXamlType(name); }
    com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() { return metadata_.GetXmlnsDefinitions(); }
    SettingsApp() {
        UnhandledException([this](auto&&, UnhandledExceptionEventArgs const& args) {
            std::ofstream log(executable_path().parent_path() / L"startup-error.log", std::ios::binary);
            log << to_string(args.Message());
            log.close();
            args.Handled(true);
            Exit();
        });
    }
    Window window_{nullptr};
    FrameworkElement page_{nullptr};
    BuildInfo own_;
    bool loading_shortcuts_{true};
    void configure_shortcuts() {
        using namespace windows_live_ime::settings;
        constexpr const wchar_t* names[]{L"HalfFull",L"NonConvert",L"Convert",L"Eisu",L"CtrlSpace",L"ShiftSpace"};
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            auto combo = page_.FindName(names[i]).as<ComboBox>();
            for (const auto* label : {L"既定",L"なし",L"IME-オフ",L"IME-オン",L"ひらがな／半角英数字",L"全角スペース"}) {
                auto item = ComboBoxItem{}; item.Content(box_value(label)); combo.Items().Append(item);
            }
            combo.SelectedIndex(static_cast<int>(std::min(read(bindings[i].value,0),DWORD(5))));
            combo.SelectionChanged([this, i](auto const& sender, auto const&) {
                if (loading_shortcuts_) return;
                const auto index = sender.template as<ComboBox>().SelectedIndex();
                if (index < 0) return;
                try { write(bindings[i].value,static_cast<DWORD>(index)); }
                catch (HRESULT error) { throw hresult_error(error); }
            });
        }
        auto enabled=page_.FindName(L"ShortcutEnabled").as<ToggleSwitch>();
        enabled.IsOn(read(L"Enabled",1)!=0);
        for (const auto* name : names) page_.FindName(name).as<ComboBox>().IsEnabled(enabled.IsOn());
        enabled.Toggled([this](auto const& sender,auto const&) {
            if (!loading_shortcuts_) try {
                const auto on=sender.template as<ToggleSwitch>().IsOn();
                write(L"Enabled",on?1:0);
                for (const auto* name : {L"HalfFull",L"NonConvert",L"Convert",L"Eisu",L"CtrlSpace",L"ShiftSpace"})
                    page_.FindName(name).as<ComboBox>().IsEnabled(on);
            }
            catch (HRESULT error) { throw hresult_error(error); }
        });
        auto physical=page_.FindName(L"PhysicalHalfFull").as<ToggleSwitch>();
        physical.IsOn(read(L"PhysicalHalfFull",1)!=0);
        physical.Toggled([this](auto const& sender,auto const&) {
            if (!loading_shortcuts_) try { write(L"PhysicalHalfFull",sender.template as<ToggleSwitch>().IsOn()?1:0); }
            catch (HRESULT error) { throw hresult_error(error); }
        });
        loading_shortcuts_=false;
    }
    void refresh() {
        const auto set = [this](const wchar_t* name, const std::wstring& text) {
            page_.FindName(name).as<TextBlock>().Text(text);
        };
        own_ = read_info(executable_path().parent_path());
        set(L"Version", identity(own_));
        set(L"BuiltAt", own_.built_at + L"（UTC）");
        const auto dll = registered_ime();
        const auto registered = read_info(dll.parent_path());
        set(L"Registered", dll.empty() ? L"このWindowsには登録されていません"
            : identity(registered) + (registered.found ? L"\n" + registered.built_at + L"（UTC）" : L""));
        set(L"RegisteredPath", dll.empty() ? L"" : dll.wstring());
        set(L"Loaded", loaded_ime_status());
    }
    void OnLaunched(LaunchActivatedEventArgs const&) {
        Resources().MergedDictionaries().Append(XamlControlsResources{});
        page_ = Markup::XamlReader::Load(LR"(
<NavigationView x:Name="Navigation" xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" PaneDisplayMode="Left" IsPaneOpen="True" IsPaneToggleButtonVisible="False" IsBackButtonVisible="Collapsed" IsSettingsVisible="False" OpenPaneLength="220" Header="Live IME 設定">
 <NavigationView.MenuItems>
  <NavigationViewItem x:Name="GeneralItem" Content="全般" Tag="general" Icon="Setting"/>
  <NavigationViewItem x:Name="ShortcutItem" Content="ショートカット" Tag="shortcuts"><NavigationViewItem.Icon><FontIcon Glyph="&#xE765;"/></NavigationViewItem.Icon></NavigationViewItem>
  <NavigationViewItem x:Name="AboutItem" Content="バージョン情報" Tag="about"><NavigationViewItem.Icon><FontIcon Glyph="&#xE946;"/></NavigationViewItem.Icon></NavigationViewItem>
 </NavigationView.MenuItems>
 <Grid>
 <ScrollViewer x:Name="GeneralPage">
  <StackPanel Margin="32" Spacing="20" MaxWidth="740" HorizontalAlignment="Stretch">
   <TextBlock Text="全般" FontSize="28" FontWeight="SemiBold"/>
   <TextBlock Text="入力モード" FontSize="18" FontWeight="SemiBold"/>
   <TextBlock Text="タスクバーの Live IME の「A／あ」を左クリックすると、ひらがなと半角英数字を切り替えます。右クリックすると入力モードと設定のメニューを開きます。" TextWrapping="Wrap"/>
   <TextBlock Text="キーの割り当ては左の「ショートカット」で変更できます。変更は自動保存され、次のキー入力から反映されます。" TextWrapping="Wrap"/>
  </StackPanel>
 </ScrollViewer>
 <ScrollViewer x:Name="ShortcutPage" Visibility="Collapsed">
  <StackPanel Margin="32" Spacing="16" MaxWidth="800">
   <TextBlock Text="ショートカット" FontSize="28" FontWeight="SemiBold"/>
   <TextBlock Text="キーの割り当て" FontSize="18" FontWeight="SemiBold"/>
   <ToggleSwitch x:Name="ShortcutEnabled" Header="各キーに好みの機能を割り当てる" OnContent="オン" OffContent="オフ（既定の割り当て）"/>
   <ComboBox x:Name="HalfFull" Header="半角／全角キー（US配列では Alt + `）" HorizontalAlignment="Stretch"/>
   <ComboBox x:Name="NonConvert" Header="無変換キー" HorizontalAlignment="Stretch"/>
   <ComboBox x:Name="Convert" Header="変換キー" HorizontalAlignment="Stretch"/>
   <ComboBox x:Name="Eisu" Header="英数キー（Caps Lock）" HorizontalAlignment="Stretch"/>
   <ComboBox x:Name="CtrlSpace" Header="Ctrl + Space" HorizontalAlignment="Stretch"/>
   <ComboBox x:Name="ShiftSpace" Header="Shift + Space" HorizontalAlignment="Stretch"/>
   <ToggleSwitch x:Name="PhysicalHalfFull" Header="US配列で認識された半角／全角キーも切り替えに使う" OnContent="オン" OffContent="オフ"/>
   <TextBlock Text="VMで半角／全角キーが ` と認識される場合の補助設定です。` を通常入力したい場合はオフにします。全角スペースは日本語入力時に挿入します。割り当て「なし」のキーはアプリへ渡します。" TextWrapping="Wrap" Opacity="0.7"/>
   <TextBlock Text="既定：半角／全角と英数は切り替え、無変換はIME-オフ、変換はIME-オン、Ctrl + Spaceはなし、Shift + Spaceは全角スペース。" TextWrapping="Wrap" Opacity="0.7"/>
  </StackPanel>
 </ScrollViewer>
 <ScrollViewer x:Name="AboutPage" Visibility="Collapsed">
 <StackPanel Margin="32" Spacing="20" MaxWidth="740" HorizontalAlignment="Stretch">
  <StackPanel Spacing="4"><TextBlock Text="Live IME" FontSize="32" FontWeight="SemiBold"/><TextBlock Text="設定 · バージョン情報" FontSize="16" Opacity="0.7"/></StackPanel>
  <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="8" Padding="20">
   <StackPanel Spacing="8"><TextBlock Text="この設定アプリ" FontWeight="SemiBold"/><TextBlock x:Name="Version" FontSize="20" IsTextSelectionEnabled="True" TextWrapping="Wrap"/><TextBlock Text="ビルド日時" Opacity="0.7"/><TextBlock x:Name="BuiltAt" IsTextSelectionEnabled="True" TextWrapping="Wrap"/></StackPanel>
  </Border>
  <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="8" Padding="20">
   <StackPanel Spacing="8"><TextBlock Text="Windowsに登録されているIME" FontWeight="SemiBold"/><TextBlock x:Name="Registered" IsTextSelectionEnabled="True" TextWrapping="Wrap"/><TextBlock x:Name="RegisteredPath" FontSize="12" Opacity="0.7" IsTextSelectionEnabled="True" TextWrapping="Wrap"/></StackPanel>
  </Border>
  <StackPanel Spacing="8"><TextBlock Text="アプリが読み込んでいるIME" FontWeight="SemiBold"/><TextBlock x:Name="Loaded" IsTextSelectionEnabled="True" TextWrapping="Wrap"/><TextBlock Text="メモ帳・Explorer・入力管理プロセスを確認します。登録済みの版と使用中の版が異なる場合は、保存してWindowsからサインアウトし、入り直してください。" TextWrapping="Wrap" Opacity="0.7"/></StackPanel>
  <Button x:Name="Refresh" Content="情報を更新" HorizontalAlignment="Left"/>
 </StackPanel>
</ScrollViewer>
 </Grid>
</NavigationView>)").as<FrameworkElement>();
        const auto navigation = page_.as<NavigationView>();
        navigation.SelectionChanged([this](NavigationView const&, NavigationViewSelectionChangedEventArgs const& args) {
            const auto about = args.SelectedItem() == page_.FindName(L"AboutItem");
            const auto shortcuts = args.SelectedItem() == page_.FindName(L"ShortcutItem");
            page_.FindName(L"GeneralPage").as<UIElement>().Visibility(about || shortcuts ? Visibility::Collapsed : Visibility::Visible);
            page_.FindName(L"AboutPage").as<UIElement>().Visibility(about ? Visibility::Visible : Visibility::Collapsed);
            page_.FindName(L"ShortcutPage").as<UIElement>().Visibility(shortcuts ? Visibility::Visible : Visibility::Collapsed);
        });
        navigation.SelectedItem(page_.FindName(L"GeneralItem"));
        configure_shortcuts();
        page_.FindName(L"Refresh").as<Button>().Click([this](auto&&, auto&&) { refresh(); });
        refresh();
        window_ = Window{};
        window_.Title(L"Live IME 設定");
        window_.Content(page_);
        window_.AppWindow().Resize({800, 720});
        window_.Activate();
        // A smoke run constructs the real WinUI window and its controls.
        if (std::wstring_view(GetCommandLineW()).find(L"--smoke-test") != std::wstring_view::npos) {
            navigation.SelectedItem(page_.FindName(L"ShortcutItem"));
            if (page_.FindName(L"ShortcutPage").as<UIElement>().Visibility() != Visibility::Visible ||
                page_.FindName(L"HalfFull").as<ComboBox>().Items().Size() != 6)
                throw hresult_error(E_FAIL,L"Shortcut navigation failed");
            const auto combo=page_.FindName(L"CtrlSpace").as<ComboBox>();
            const auto original=windows_live_ime::settings::read(L"CtrlSpace",0);
            const auto originalIndex=combo.SelectedIndex();
            const auto changed=originalIndex==4?3:4;
            combo.SelectedIndex(changed);
            const bool saved=windows_live_ime::settings::read(L"CtrlSpace",99)==static_cast<DWORD>(changed);
            combo.SelectedIndex(originalIndex);
            windows_live_ime::settings::write(L"CtrlSpace",original);
            if (!saved) throw hresult_error(E_FAIL,L"Shortcut UI persistence failed");
            navigation.SelectedItem(page_.FindName(L"AboutItem"));
            if (page_.FindName(L"AboutPage").as<UIElement>().Visibility() != Visibility::Visible)
                throw hresult_error(E_FAIL, L"Version navigation failed");
            auto result = Windows::Data::Json::JsonObject{};
            result.SetNamedValue(L"winuiWindowCreated", Windows::Data::Json::JsonValue::CreateBooleanValue(true));
            result.SetNamedValue(L"shortcutSettingsSaved", Windows::Data::Json::JsonValue::CreateBooleanValue(saved));
            result.SetNamedValue(L"version", Windows::Data::Json::JsonValue::CreateStringValue(own_.version));
            result.SetNamedValue(L"commit", Windows::Data::Json::JsonValue::CreateStringValue(own_.commit));
            result.SetNamedValue(L"displayedVersion", Windows::Data::Json::JsonValue::CreateStringValue(page_.FindName(L"Version").as<TextBlock>().Text()));
            std::ofstream report(executable_path().parent_path() / L"smoke-test.json", std::ios::binary);
            report << to_string(result.Stringify());
            report.close();
            window_.Close();
            Exit();
        }
    }
};
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        Application::Start([](auto&&) { winrt::make<SettingsApp>(); });
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::ofstream log(executable_path().parent_path() / L"startup-error.log", std::ios::binary);
        log << "HRESULT 0x" << std::hex << static_cast<std::uint32_t>(error.code()) << "\n" << winrt::to_string(error.message());
        return 1;
    }
}
