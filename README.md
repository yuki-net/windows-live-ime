# Windows Live IME

Windows上で、macOSのライブ変換に近い入力体験を目指す日本語IMEです。

変換精度だけではなく、入力中にIMEの存在を意識しにくい低遅延なライブ変換、自然な候補操作、Windows 11に馴染むネイティブUIを重視します。

> [!NOTE]
> 現在は初期開発段階です。プロジェクト名 `windows-live-ime` は仮称です。

## 開発版の入力操作

### バージョン確認

VM内でLive IMEを選び、タスクバーの「A／あ」を右クリックして「設定」を開きます。左サイドバーの「バージョン情報」にバージョン、Gitコミット、ビルド日時が表示されます。「Windowsに登録されているIME」と「アプリが読み込んでいるIME」も確認できるので、旧版が残っている場合に区別できます。「情報を更新」で再確認します。VMのデスクトップにある「Live IME 設定」からも開けます。

右クリックメニューの「ひらがな」「半角英数字」は入力モードを切り替えます。全角カタカナ・全角英数字・半角カタカナの固定入力モード、単語の追加、プライベートモードは「（未実装）」付きの無効な項目です。F6〜F10による入力中の文字種変換とは別の機能です。

設定の「ショートカット」で半角／全角・無変換・変換・英数・Ctrl+Space・Shift+Spaceへ機能を割り当てます。変更は同じWindowsユーザーのレジストリへ自動保存し、次の入力から反映します。VMで半角／全角がUS配列の`として認識される場合の補助設定もあります。Ctrl+Spaceの既定は「なし」で、切り替えに使いたい場合は「ひらがな／半角英数字」を選びます。

キーの役割は[Microsoft IMEの公式説明](https://support.microsoft.com/ja-jp/windows/hardware/input-devices/microsoft-japanese-ime)を参考にしています。無変換のIME-オフ／変換のIME-オンはこの開発版の既定で、カスタマイズできます。

設定画面単体の開発では、CLIから `scripts/run-settings.ps1 -Configuration Release` を実行してホストでビルド・起動できます。これはIMEを登録しません。ビルドだけ行う場合は `scripts/build-settings.ps1 -Configuration Release` を使用します。通常のIME確認ではVM内の「あ／A」を右クリックして設定を開きます。

WinUI 3のビルドに必要なNuGetパッケージは `build/nuget/` に取得します。Windows App SDKのビルド処理用Roslynコンパイラもここへ取得するため、Visual StudioにC#ワークロードを追加する必要はありません。設定アプリの言語はC++/WinRTです。

### 入力

VM内で「Live IME」を選んで使用します。現在の入力実装は実験段階で、実アプリでの操作確認は継続中です。

| 操作 | キー |
|---|---|
| 日本語／英数の切り替え | 半角／全角、英数、US配列ではAlt+`、言語バーの「あ／A」 |
| 日本語へ切り替え | 変換 |
| 英数へ切り替え | 無変換 |
| 候補表示・次候補 | Space、↓ |
| 前候補 | ↑ |
| 候補ページ移動 | PageUp / PageDown |
| 候補を選んで確定 | 1〜9 |
| 確定 | Enter |
| 候補選択解除・入力取消 | Esc |
| ひらがな／カタカナ／半角カナ／全角英数／半角英数 | F6〜F10 |

日本語モードではローマ字入力をかなへ変え、別プロセスのエンジンから届いた最新の候補を入力中に反映します。

## Goals

- Windowsで低遅延なライブ変換を実現する
- macOS日本語入力に近い自然な入力・候補選択体験を再現する
- `変換` キーで日本語入力、`無変換` キーで英数入力へ切り替えられるようにする
- ひらがな・カタカナ・英数などの文字種変換を、辞書候補とは独立した安定した操作として提供する
- Windows 11に馴染む軽量でネイティブな候補UIを提供する
- Microsoft IMEから取得可能な設定は初期値へ反映し、独自設定を極力増やさない
- 変換エンジンと入力体験・UIを分離し、将来のエンジン差し替えを可能にする

## Non-goals for v1

以下は初期スコープに含めません。

- 独自かな漢字変換エンジンの開発
- 最新語辞書の自動配信・継続更新
- クラウド変換
- 高度なユーザー学習
- Linux対応
- macOS純正IMEのバイナリや内部実装の流用

Linux対応は、Windows版が安定した後に必要性を判断します。OS非依存にできるロジックは `core` に閉じ込め、将来的な再利用を妨げない構造にします。

## Tech Stack

### Core / Windows IME

- C++23
- CMake
- Text Services Framework (TSF)
- COM / Win32
- Direct2D
- DirectWrite
- Desktop Window Manager (DWM)

TSF DLLは可能な限り小さく保ち、入力処理・状態管理・候補表示・変換エンジンとの通信に集中させます。

### Conversion Engine

- Swift 6 for Windows
- Swift Package Manager
- AzooKeyKanaKanjiConverter

AzooKeyKanaKanjiConverterはIME本体とは別プロセスの変換ホストから利用する想定です。TSF DLLへSwiftランタイムを直接組み込みません。

### Settings

- C++/WinRT
- Windows App SDK / WinUI 3

設定画面はIME本体から分離し、必要なときだけ起動します。

## Architecture

```text
Windows application
       |
       | TSF
       v
+----------------------------+
| Windows IME (C++23)        |
|                            |
|  platform/windows          |
|  - TSF / COM               |
|  - key event integration   |
|  - Direct2D / DirectWrite  |
|  - settings integration    |
+-------------+--------------+
              |
              v
+----------------------------+
| Core (C++23)               |
|                            |
|  - composition state       |
|  - live conversion         |
|  - candidate model         |
|  - key actions             |
|  - conversion interface    |
+-------------+--------------+
              |
              | IPC
              v
+----------------------------+
| Conversion Engine Host     |
| Swift 6                    |
|                            |
| AzooKeyKanaKanjiConverter  |
+----------------------------+
```

ライブ変換では「ユーザーが入力を止めるまで待つ」ことを基本設計にしません。変換要求にgenerationを付与し、入力が進んだ場合は古い結果を破棄して最新結果だけを表示する方式を基本とします。

## Repository Layout

```text
.
├─ core/
│  ├─ include/
│  ├─ src/
│  └─ tests/
├─ engine/
│  └─ azookey/
├─ platform/
│  └─ windows/
│     ├─ ime/
│     ├─ renderer/
│     ├─ settings/
│     └─ installer/
├─ docs/
├─ CMakeLists.txt
└─ README.md
```

### core

OS固有APIに依存しない入力ロジックを置きます。

- Input / composition state
- Live conversion coordinator
- Candidate model
- Key actions
- Conversion engine abstraction

Windows API、TSF、HWND、Direct2D、WinUIなどへの依存は置きません。

### engine

かな漢字変換エンジンとの接続を置きます。初期実装ではAzooKeyKanaKanjiConverterを利用します。

### platform/windows

Windows固有実装を置きます。

- TSF / COM
- Candidate renderer
- Windows設定との連携
- Settings UI
- Installer / IME registration

### docs

設計判断や挙動仕様を記録します。特にmacOS日本語入力を参考にした挙動は、実装コードとは分けて仕様として整理します。

## Input UX

初期段階では以下の操作を目標にします。

- 入力中はライブ変換を継続する
- `変換` : 日本語入力をON
- `無変換` : 英数入力へ切り替え
- 下キーなどから通常候補選択へ移行
- ひらがな / カタカナ / 半角英数 / 全角英数を安定した文字種変換として扱う
- 候補順位や学習状態に文字種変換の位置を依存させない

詳細なキー挙動はmacOS日本語入力とMicrosoft IMEを実機比較しながら仕様化します。

## UI

候補UIはWindows 11に馴染むことを既定とします。

IME本体の候補表示にはWinUIを直接使用せず、Win32 HWND上へDirect2D / DirectWriteで描画する方針です。

将来的には候補モデルと描画を分離し、例えば以下の表示テーマを切り替えられる設計も検討します。

- Windows / Fluent
- Compact / macOS-inspired

操作ロジックとテーマは分離します。

## Development

### 必要な環境

- Windows 11 x64
- Visual Studio 2022 の MSVC x64/x86 build tools
- Windows 11 SDK
- CMake 3.25 以上、Ninja
- Swift for Windows 6.1 以上
- PowerShell 5.1 と Git for Windows
- Hyper-V と Windows 11 x64 の開発VM（VM統合サービスを有効にする）
- JetBrains CLion

ビルドスクリプトは Visual Studio の開発環境を初期化し、CMake/Ninja は PATH または CLion/Visual Studio の標準配置先から探します。Swift for Windows は PATH または公式インストーラーの標準配置先から探し、Windows SDK の `SDKROOT` も自動設定します。Windows SDK は TSF DLL と Swift toolchain が使います。

AzooKeyKanaKanjiConverter は revision `80b8204f1cdfb364bb2ed355cf52c7ebb2519a0c`（`v0.11.2`）へ固定しています。このパッケージの manifest は Windows では一部依存を除外しますが、README の検証済みOS一覧に Windows は含まれていません。Windows runner のCIでビルドと最小変換を確認します。

### C++ build / tests

```powershell
scripts/build.ps1 -Configuration Debug
scripts/test.ps1 -Configuration Debug -NoBuild
```

`scripts/test.ps1` は Core/IPC の CTest、Swift の unit tests、AzooKey の初期化と最小変換を確認します。engine hostのself-checkはSwift/MSVC runtime DLLのPATH依存を外して実行し、VMへ配布する成果物だけで起動することも確認します。`-NoBuild` を外すと、先に build も実行します。`-CoreOnly` は TSF DLL を除いた Core/IPC 検証に使います。

### Git pre-push hook

```powershell
scripts/setup-hooks.ps1
```

hook は Core/IPC の build と tests、engine host の build/check を実行します。VM 起動やIME登録は行いません。標準 Git の `--no-verify` 以外の bypass は設けていません。

### Hyper-V development VM

スクリプトは既存の Windows 11 x64 VM を使用します。初回は VM のローカル管理者アカウントを現在のWindowsユーザーで暗号化保存し、VM名を既定値 `WindowsLiveImeDev` に合わせます。engine host、Swift runtime DLL、MSVC x64 runtime DLLを同じ build 世代のフォルダーに配置するため、VMにSwift toolchainやVisual Studioをインストールする必要はありません。

```powershell
scripts/vm/setup-credential.ps1
scripts/vm/setup.ps1
```

資格情報は `%LOCALAPPDATA%\windows-live-ime\vm-credential.xml` にユーザー単位で保護して保存され、リポジトリへ書き込みません。ホストとVMの間は PowerShell Direct を使うため、SSH、WinRM、固定IPは不要です。VMは事前に作成し、Windows 11をインストールしておく必要があります。

CLion の `Dev IME` 共有Run Configurationは `scripts/dev.ps1` だけを呼びます。スクリプトがbuild、VM起動、世代別deploy、旧登録解除、新登録、engine-host起動とNamed Pipe health check、Notepad起動、VMConnectまで進めます。

```powershell
scripts/dev.ps1 -VMName WindowsLiveImeDev -Configuration Release
```

各deployは `C:\windows-live-ime-dev\builds\000001` のような別ディレクトリへ配置します。TSF DLLを同じ場所へ上書きしません。`scripts/vm/reset.ps1` は登録を解除してVMをシャットダウンし、deploy世代は保持します。任意のVM checkpointへ戻す場合は `-CheckpointName <name>` を指定します。

共有Run Configurationは `.run/` の `Core Tests` と `Dev IME` に限定しています。`Core Tests` は build/test、`Dev IME` はVMでのIME確認を行います。VMの登録解除・シャットダウンが必要な保守作業では、CLIから `scripts/vm/reset.ps1` を実行します。

### CI

GitHub Actions は Windows 2022 runner で、MSVC/CMake/Ninja、Swift 6.1.3、Core、TSF DLL、IPC protocol、engine host と AzooKey 依存をbuild/testします。Pull Request と `develop` へのpushで動きます。

## License

未定。
