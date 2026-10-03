# 初期アーキテクチャ

## Core と Windows 境界

`core/` は標準C++23のみを使用します。入力/Composition state、Candidate、KeyAction、conversion request/response、generation ID、非同期 `ConversionEngine` abstraction を定義し、Windows SDKやCOMを含めません。

新しい入力ごとに request ID と generation ID を発行します。応答を反映する前に `LiveConversionCoordinator::is_current` で generation を確認し、古い結果を破棄します。変換engineの `submit` は処理をqueueしてすぐ戻る契約です。完了callbackはworker threadで呼ばれる場合があり、呼び出し側がTSF/UI threadへmarshalします。

`platform/windows/ime/` はCOM class factory、TSF text service、DLL registration、Named Pipe clientを保持します。`platform/windows/renderer/candidate_renderer.hpp` はCore CandidateをWindows描画層へ渡す境界で、Direct2D/DirectWrite描画本体は後続Issueで実装します。

初期TSF serviceはロードと登録を担い、まだキーイベントや変換を処理しません。Named Pipe protocolと診断clientは独立してbuild/testできるため、後続IssueでTSF側の非同期変換接続を追加できます。

## Engine host

`engine/azookey/` は独立した SwiftPM executable `engine-host.exe` を生成します。Swift runtimeはTSF DLLへリンクしません。hostはNamed Pipe serverとしてhealth pingとconversion requestを受け取り、AzooKey候補を返します。

AzooKeyKanaKanjiConverterは `v0.11.2` のrevision `80b8204f1cdfb364bb2ed355cf52c7ebb2519a0c` に固定しています。初期hostでは既定辞書を初期化し、`LOCALAPPDATA\WindowsLiveIME\Engine` をユーザーデータ領域として使います。

## Buildとtest

`CMakePresets.json` はWindows x64向けNinja Multi-Config presetを定義します。`scripts/build.ps1` はMSVC環境を初期化し、CMakeとSwiftPMを同じ入口からbuildします。CIとpre-pushは `scripts/test.ps1` を共有します。

## Hyper-V

ホストはHyper-V PowerShell moduleでVMを起動し、`New-PSSession -VMName`、`Copy-Item -ToSession`、`Invoke-Command` で操作します。VM資格情報は現在のWindowsユーザーのDPAPI保護を使って `%LOCALAPPDATA%` に保存します。deploy generationごとに別フォルダーを使い、登録中のDLLを上書きしません。
