import EngineCore
import EngineHost
import Foundation

@main
struct EngineHostMain {
    static func main() throws {
        let arguments = Array(CommandLine.arguments.dropFirst())
        let engine = try KanaKanjiEngine()

        if arguments.contains("--self-check") {
            let candidates = engine.convert("かな")
            guard !candidates.isEmpty else {
                throw EngineError.noCandidates
            }
            print("AzooKey initialized and returned \(candidates.count) candidate(s).")
            return
        }

        let pipeName = ProcessInfo.processInfo.environment["WINDOWS_LIVE_IME_PIPE_NAME"]
            ?? #"\\.\pipe\windows-live-ime"#
        print("Live IME engine host is listening.")
        try NamedPipeEngineHost(engine: engine).run(pipeName: pipeName)
    }
}
