import EngineCore
import Foundation
import WinSDK

public final class NamedPipeEngineHost {
    private let engine: KanaKanjiEngine

    public init(engine: KanaKanjiEngine) {
        self.engine = engine
    }

    public func run(pipeName: String = #"\\.\pipe\windows-live-ime"#) throws {
        let wideName = Array(pipeName.utf16) + [0]
        let pipe = wideName.withUnsafeBufferPointer { buffer in
            CreateNamedPipeW(
                buffer.baseAddress,
                DWORD(PIPE_ACCESS_DUPLEX),
                DWORD(PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS),
                1,
                64 * 1024,
                64 * 1024,
                0,
                nil
            )
        }
        guard pipe != INVALID_HANDLE_VALUE else {
            throw EngineHostError.pipeCreateFailed
        }
        defer { _ = CloseHandle(pipe) }

        while true {
            guard ConnectNamedPipe(pipe, nil) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED else {
                throw EngineHostError.pipeAcceptFailed(Int32(bitPattern: GetLastError()))
            }
            do {
                let request = try readMessage(from: pipe)
                let response = try handle(request)
                let bytes = try response.encoded()
                try write(bytes, to: pipe)
            } catch {
                print("Named Pipe request failed: \(error)")
            }
            _ = DisconnectNamedPipe(pipe)
        }
    }

    private func readMessage(from pipe: HANDLE) throws -> PipeMessage {
        let header = try readBytes(from: pipe, count: PipeMessage.headerSize)
        let payloadSize = try PipeMessage.payloadSize(fromHeader: header)
        let payload = try readBytes(from: pipe, count: payloadSize)
        return try PipeMessage.decode(header + payload)
    }

    private func readBytes(from pipe: HANDLE, count: Int) throws -> Data {
        guard count > 0 else {
            return Data()
        }
        var bytes = Data(count: count)
        var offset = 0
        let deadline = GetTickCount64() + 5_000
        while offset < count {
            var available: DWORD = 0
            guard PeekNamedPipe(pipe, nil, 0, nil, &available, nil) != FALSE else {
                throw EngineHostError.pipeReadFailed(Int32(bitPattern: GetLastError()))
            }
            guard available > 0 else {
                guard GetTickCount64() < deadline else {
                    throw EngineHostError.pipeReadFailed(Int32(bitPattern: ERROR_SEM_TIMEOUT))
                }
                Sleep(10)
                continue
            }

            var transferred: DWORD = 0
            let chunkSize = DWORD(min(count - offset, Int(available)))
            let succeeded = bytes.withUnsafeMutableBytes { buffer in
                ReadFile(
                    pipe,
                    buffer.baseAddress!.advanced(by: offset),
                    chunkSize,
                    &transferred,
                    nil
                )
            }
            guard succeeded != FALSE else {
                throw EngineHostError.pipeReadFailed(Int32(bitPattern: GetLastError()))
            }
            guard transferred > 0 else {
                throw EngineHostError.pipeReadFailed(Int32(bitPattern: ERROR_BROKEN_PIPE))
            }
            offset += Int(transferred)
        }
        return bytes
    }

    private func write(_ bytes: Data, to pipe: HANDLE) throws {
        var offset = 0
        while offset < bytes.count {
            var transferred: DWORD = 0
            let succeeded = bytes.withUnsafeBytes { buffer in
                WriteFile(
                    pipe,
                    buffer.baseAddress!.advanced(by: offset),
                    DWORD(bytes.count - offset),
                    &transferred,
                    nil
                )
            }
            guard succeeded != FALSE else {
                throw EngineHostError.pipeWriteFailed(Int32(bitPattern: GetLastError()))
            }
            guard transferred > 0 else {
                throw EngineHostError.pipeWriteFailed(Int32(bitPattern: ERROR_BROKEN_PIPE))
            }
            offset += Int(transferred)
        }
    }

    private func handle(_ request: PipeMessage) throws -> PipeMessage {
        switch request.kind {
        case .ping:
            return PipeMessage(
                kind: .pong,
                requestID: request.requestID,
                generationID: request.generationID
            )
        case .convert:
            guard let input = String(data: request.payload, encoding: .utf8) else {
                return errorResponse(for: request, status: 1, description: "Input is not valid UTF-8.")
            }
            let candidates = engine.convert(input).map {
                WireCandidate(text: $0, annotation: "")
            }
            return PipeMessage(
                kind: .convertResult,
                requestID: request.requestID,
                generationID: request.generationID,
                payload: try encodeCandidates(candidates)
            )
        case .pong, .convertResult, .error:
            return errorResponse(for: request, status: 2, description: "Unexpected request kind.")
        }
    }

    private func errorResponse(for request: PipeMessage, status: UInt32, description: String) -> PipeMessage {
        PipeMessage(
            kind: .error,
            requestID: request.requestID,
            generationID: request.generationID,
            status: status,
            payload: Data(description.utf8)
        )
    }
}

private enum EngineHostError: Error {
    case pipeCreateFailed
    case pipeAcceptFailed(Int32)
    case pipeReadFailed(Int32)
    case pipeWriteFailed(Int32)
}
