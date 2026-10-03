import Foundation
import KanaKanjiConverterModuleWithDefaultDictionary

public final class KanaKanjiEngine {
    private let converter: KanaKanjiConverter
    private let storageDirectory: URL

    public init() throws {
        guard let localAppData = ProcessInfo.processInfo.environment["LOCALAPPDATA"] else {
            throw EngineError.missingLocalAppData
        }
        storageDirectory = URL(fileURLWithPath: localAppData, isDirectory: true)
            .appending(path: "WindowsLiveIME", directoryHint: .isDirectory)
            .appending(path: "Engine", directoryHint: .isDirectory)
        try FileManager.default.createDirectory(
            at: storageDirectory,
            withIntermediateDirectories: true
        )
        converter = KanaKanjiConverter.withDefaultDictionary()
    }

    public func convert(_ input: String) -> [String] {
        var composingText = ComposingText()
        composingText.insertAtCursorPosition(input, inputStyle: .direct)

        let options = ConvertRequestOptions(
            requireJapanesePrediction: true,
            requireEnglishPrediction: false,
            keyboardLanguage: .ja_JP,
            learningType: .nothing,
            memoryDirectoryURL: storageDirectory,
            sharedContainerURL: storageDirectory,
            metadata: .init(versionString: "Windows Live IME"),
            textReplacer: .withDefaultEmojiDictionary(),
            specialCandidateProviders: KanaKanjiConverter.defaultSpecialCandidateProviders
        )
        let results = converter.requestCandidates(composingText, options: options)
        return results.mainResults.map(\.text)
    }
}

public enum EngineError: Error {
    case missingLocalAppData
    case noCandidates
}
