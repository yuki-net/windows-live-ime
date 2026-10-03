# Windows Live IME

Windows上で、macOSのライブ変換に近い入力体験を目指す日本語IMEです。

変換精度だけではなく、入力中にIMEの存在を意識しにくい低遅延なライブ変換、自然な候補操作、Windows 11に馴染むネイティブUIを重視します。

> [!NOTE]
> 現在は初期開発段階です。プロジェクト名 `windows-live-ime` は仮称です。

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

初期構築中です。ビルド・インストール・デバッグ手順は基盤実装と合わせて追加します。

IMEの不具合は利用中アプリケーションへ影響する可能性があるため、初期開発ではVMまたは開発専用環境での検証を推奨します。

## License

未定。
