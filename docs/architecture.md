# 初期アーキテクチャ

## Core と Windows 境界

`core/` は標準C++23のみを使用します。入力/Composition state、Candidate、KeyAction、conversion request/response、generation ID、非同期 `ConversionEngine` abstraction を定義し、Windows SDKやCOMを含めません。

新しい入力ごとに request ID と generation ID を発行します。応答を反映する前に `LiveConversionCoordinator::is_current` で generation を確認し、古い結果を破棄します。変換engineの `submit` は処理をqueueしてすぐ戻る契約です。完了callbackはworker threadで呼ばれる場合があり、呼び出し側がTSF/UI threadへmarshalします。

`platform/windows/ime/` はCOM class factory、TSF text service、DLL registration、Named Pipe clientを保持します。`platform/windows/renderer/` はDirect2D/DirectWriteによる非アクティブ化候補ウィンドウを保持します。

TSF serviceはキーイベントを受け取り、Coreのローマ字解析結果をedit sessionでcompositionへ反映します。変換は専用workerからNamed Pipeへ要求し、結果をmessage window経由でTSF threadへ返します。generationが一致する最新結果だけを適用し、キー入力中に変換完了を待ちません。エンジン不通時にはかな入力を継続します。

入力モードはTSFのopen/close・conversion compartmentと「あ／A」言語バーボタンへ反映します。英数モードでは文字キーをアプリへ渡します。候補選択はSpace・上下キー・数字キーで操作し、Enterで確定、Escで候補選択解除または入力取消を行います。F6〜F10は文字種変換です。

## Engine host

`engine/azookey/` は独立した SwiftPM executable `engine-host.exe` を生成します。Swift runtimeはTSF DLLへリンクしません。hostはNamed Pipe serverとしてhealth pingとconversion requestを受け取り、AzooKey候補を返します。

AzooKeyKanaKanjiConverterは `v0.11.2` のrevision `80b8204f1cdfb364bb2ed355cf52c7ebb2519a0c` に固定しています。初期hostでは既定辞書を初期化し、`LOCALAPPDATA\WindowsLiveIME\Engine` をユーザーデータ領域として使います。

## Buildとtest

`CMakePresets.json` はWindows x64向けNinja Multi-Config presetを定義します。`scripts/build.ps1` はMSVC環境を初期化し、CMakeとSwiftPMを同じ入口からbuildします。CIとpre-pushは `scripts/test.ps1` を共有します。

## Hyper-V

ホストはHyper-V PowerShell moduleでVMを起動し、`New-PSSession -VMName`、`Copy-Item -ToSession`、`Invoke-Command` で操作します。VM資格情報は現在のWindowsユーザーのDPAPI保護を使って `%LOCALAPPDATA%` に保存します。deploy generationごとに別フォルダーを使い、登録中のDLLを上書きしません。
