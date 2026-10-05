# 初期アーキテクチャ

## Core と Windows 境界

`core/` は標準C++23のみを使用します。入力/Composition state、Candidate、KeyAction、conversion request/response、generation ID、非同期 `ConversionEngine` abstraction を定義し、Windows SDKやCOMを含めません。

新しい入力ごとに request ID と generation ID を発行します。応答を反映する前に `LiveConversionCoordinator::is_current` で generation を確認し、古い結果を破棄します。変換engineの `submit` は処理をqueueしてすぐ戻る契約です。完了callbackはworker threadで呼ばれる場合があり、呼び出し側がTSF/UI threadへmarshalします。

`platform/windows/ime/` はCOM class factory、TSF text service、DLL registration、Named Pipe clientを保持します。`platform/windows/renderer/` はDirect2D/DirectWriteによる非アクティブ化候補ウィンドウを保持します。

TSF serviceはキーイベントを受け取り、Coreのローマ字解析結果をedit sessionでcompositionへ反映します。変換は専用workerからNamed Pipeへ要求し、結果をmessage window経由でTSF threadへ返します。generationが一致する最新結果だけを適用し、キー入力中に変換完了を待ちません。エンジン不通時にはかな入力を継続します。

入力モードはTSFのopen/close・conversion compartmentと「あ／A」言語バーボタンへ反映します。英数モードでは文字キーをアプリへ渡します。候補選択はSpace・上下キー・数字キーで操作し、Enterで確定、Escで候補選択解除または入力取消を行います。F6〜F10は文字種変換です。

半角／全角・無変換・変換・英数・Ctrl+Space・Shift+SpaceはTSFのPreserveKeyへ登録し、英数モードでも切り替えを受け取ります。設定アプリとDLLは`platform/windows/shared/shortcut_settings.hpp`の同じ割り当て定義とHKCUレジストリ値を使用します。設定は次の入力時に読み直します。US配列のAlt+`と、VMでJISキーが`として届く場合の補助設定を提供します。

変換結果は候補選択を開始した後でも、generationが一致する場合に反映します。末尾nをSpaceで変換するときは「ん」を含む読みを改めてエンジンへ要求します。IPCの待機はworker上で行い、cold conversion向けのtimeoutは3秒です。

## Engine host

`engine/azookey/` は独立した SwiftPM executable `engine-host.exe` を生成します。Swift runtimeはTSF DLLへリンクしません。hostはNamed Pipe serverとしてhealth pingとconversion requestを受け取り、AzooKey候補を返します。

AzooKeyKanaKanjiConverterは `v0.11.2` のrevision `80b8204f1cdfb364bb2ed355cf52c7ebb2519a0c` に固定しています。初期hostでは既定辞書を初期化し、`LOCALAPPDATA\WindowsLiveIME\Engine` をユーザーデータ領域として使います。

Named Pipeはengine hostのユーザーSID・System・Administratorsへのアクセスを明示し、通常権限の同じユーザーから利用できるmedium integrity labelを付けます。応答を書いた後はFlushFileBuffersで受信を確認してからDisconnectNamedPipeを行います。`ime_ipc_ping.exe -Convert <かな>`は実際の変換要求・候補の復号・request/generation ID一致を確認し、漢字候補の有無を終了コードで報告します。

## Settings app

`platform/windows/settings/` は独立した C++/WinRT / WinUI 3 アプリです。TSF DLLにはWinUIをリンクしません。Windows App SDK `1.8.260921001` と C++/WinRT `2.0.250303.1`、その依存パッケージを `packages.config` に固定します。

設定アプリはunpackaged/self-containedでビルドし、exeとWinUI runtimeを同じフォルダーに配置します。NavigationViewの左サイドバーは「全般」「バージョン情報」です。全般は入力操作の案内、バージョン情報は設定アプリ自身の版、ビルド日時、COM登録先のIME DLLの版、メモ帳・Explorer・ctfmonが読み込んでいるIME DLLの版を表示します。確認できない場合は情報なしと表示し、登録先を使用中の版と見なしません。

TSFのModeButton右クリックメニューから設定アプリを起動します。読み込まれているDLL自身のフォルダーを基準にsettings/LiveImeSettings.exeを解決し、同じWindows・ユーザーのプロセスとして起動します。ひらがなと半角英数字は既存の入力モード処理へ接続します。他の固定入力モード、単語登録、プライベートモードは未実装と明記して無効にします。

ModeButtonのOnClick(TF_LBI_CLK_RIGHT)はネイティブのポップアップを直接開きます。TF_LBI_STYLE_BTN_MENUは言語バーのドロップダウン用であり、右クリック表示の代替にはなりません。ime_mode_button_testsは実際のOnClickを呼び、Windowsがメニューモードへ入ったことを確認して自動で閉じます。

`VERSION` が製品バージョンの元です。ビルド時にGit commit、未コミット変更の有無、UTC日時を `build-info.json` に記録します。同じdeployに含まれるIMEと設定アプリには同じ情報ファイルを配置します。古いdeployには情報ファイルがないため、旧版または情報なしとして表示します。

`scripts/build-settings.ps1` は設定アプリのみをMSBuildでビルドします。ビルド環境にはMSVC、MSBuild、Windows SDK 26100が必要です。Windows App SDKのinline MSBuildタスク用Roslyn compilerは `Microsoft.Net.Compilers.Toolset 4.14.0` としてbuild内に取得します。ホストへのコンポーネント追加を必須にせず、アプリ自体はC++です。CLionの `Settings App` 構成は設定アプリのみを起動します。

通常のWindows buildは設定アプリも `artifacts/<configuration>/settings/` へ配置します。VMのregisterはデスクトップの「Live IME 設定」ショートカットを、そのdeployの設定exeへ更新します。設定アプリを開くだけではIMEの登録を変更しません。

`scripts/test-settings.ps1` は開発ツールのPATHを除いて実際のWinUIウィンドウを生成し、画面のTextBlockに入った版情報と `build-info.json` の一致を確認して終了します。IME入力のGUI確認を代替するテストではありません。

## Buildとtest

`CMakePresets.json` はWindows x64向けNinja Multi-Config presetを定義します。`scripts/build.ps1` はMSVC環境を初期化し、CMakeとSwiftPMを同じ入口からbuildします。CIとpre-pushは `scripts/test.ps1` を共有します。

## Hyper-V

ホストはHyper-V PowerShell moduleでVMを起動し、`New-PSSession -VMName`、`Copy-Item -ToSession`、`Invoke-Command` で操作します。VM資格情報は現在のWindowsユーザーのDPAPI保護を使って `%LOCALAPPDATA%` に保存します。deploy generationごとに別フォルダーを使い、登録中のDLLを上書きしません。

VM転送は実行用DLL・exe・Swift resource bundle・設定アプリをZIP一個にまとめ、PowerShell Directで転送してゲスト内で展開します。Swiftのobject/module・テスト生成物・PDBは転送しません。ファイルごとの進捗表示は抑止し、準備・転送・展開のログと所要時間を表示します。WinUIのNuGet依存は不足時のみrestoreし、手動で再restoreする場合はbuild-settings.ps1に`-Restore`を指定します。
