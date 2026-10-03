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
            N_best: 10,
            requireJapanesePrediction: true,
            requireEnglishPrediction: false,
            keyboardLanguage: .ja_JP,
            englishCandidateInRoman2KanaInput: true,
            fullWidthRomanCandidate: false,
            halfWidthKanaCandidate: false,
            learningType: .nothing,
            maxMemoryCount: 0,
            shouldResetMemory: false,
            memoryDirectoryURL: storageDirectory,
            sharedContainerURL: storageDirectory,
            textReplacer: .withDefaultEmojiDictionary(),
            specialCandidateProviders: KanaKanjiConverter.defaultSpecialCandidateProviders,
            zenzaiMode: .off,
            preloadDictionary: false,
            metadata: .init(versionString: "Windows Live IME")
        )
        let results = converter.requestCandidates(composingText, options: options)
        return results.mainResults.map { $0.text }
    }
}

public enum EngineError: Error {
    case missingLocalAppData
    case noCandidates
}
