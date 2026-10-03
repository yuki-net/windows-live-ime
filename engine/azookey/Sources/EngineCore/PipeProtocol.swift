import Foundation

public enum PipeMessageKind: UInt16, Equatable {
    case ping = 1
    case pong = 2
    case convert = 3
    case convertResult = 4
    case error = 5
}

public struct PipeMessage: Equatable {
    public static let protocolVersion: UInt16 = 1
    public static let headerSize = 32
    public static let maximumPayloadSize = 1024 * 1024
    public static let maximumCandidateCount: UInt32 = 1024

    public var kind: PipeMessageKind
    public var requestID: UInt64
    public var generationID: UInt64
    public var status: UInt32
    public var payload: Data

    public init(
        kind: PipeMessageKind,
        requestID: UInt64,
        generationID: UInt64,
        status: UInt32 = 0,
        payload: Data = Data()
    ) {
        self.kind = kind
        self.requestID = requestID
        self.generationID = generationID
        self.status = status
        self.payload = payload
    }

    public func encoded() throws -> Data {
        guard payload.count <= Self.maximumPayloadSize else {
            throw PipeProtocolError.payloadTooLarge
        }

        var bytes = Data([0x57, 0x49, 0x4D, 0x45]) // WIME
        bytes.appendLittleEndian(Self.protocolVersion)
        bytes.appendLittleEndian(kind.rawValue)
        bytes.appendLittleEndian(UInt32(payload.count))
        bytes.appendLittleEndian(requestID)
        bytes.appendLittleEndian(generationID)
        bytes.appendLittleEndian(status)
        bytes.append(payload)
        return bytes
    }

    public static func payloadSize(fromHeader header: Data) throws -> Int {
        guard header.count == headerSize else {
            throw PipeProtocolError.invalidHeader
        }
        guard header.prefix(4) == Data([0x57, 0x49, 0x4D, 0x45]) else {
            throw PipeProtocolError.badMagic
        }
        guard try header.readUInt16LE(at: 4) == protocolVersion else {
            throw PipeProtocolError.unsupportedVersion
        }
        guard PipeMessageKind(rawValue: try header.readUInt16LE(at: 6)) != nil else {
            throw PipeProtocolError.invalidMessageKind
        }

        let size = try header.readUInt32LE(at: 8)
        guard size <= maximumPayloadSize else {
            throw PipeProtocolError.payloadTooLarge
        }
        return Int(size)
    }

    public static func decode(_ bytes: Data) throws -> PipeMessage {
        guard bytes.count >= headerSize else {
            throw PipeProtocolError.invalidHeader
        }
        let header = bytes.prefix(headerSize)
        let payloadSize = try payloadSize(fromHeader: Data(header))
        guard bytes.count == headerSize + payloadSize else {
            throw PipeProtocolError.lengthMismatch
        }

        guard let kind = PipeMessageKind(rawValue: try header.readUInt16LE(at: 6)) else {
            throw PipeProtocolError.invalidMessageKind
        }
        return PipeMessage(
            kind: kind,
            requestID: try header.readUInt64LE(at: 12),
            generationID: try header.readUInt64LE(at: 20),
            status: try header.readUInt32LE(at: 28),
            payload: Data(bytes.dropFirst(headerSize))
        )
    }
}

public struct WireCandidate: Equatable {
    public var text: String
    public var annotation: String

    public init(text: String, annotation: String) {
        self.text = text
        self.annotation = annotation
    }
}

public enum PipeProtocolError: Error, Equatable {
    case invalidHeader
    case badMagic
    case unsupportedVersion
    case invalidMessageKind
    case payloadTooLarge
    case lengthMismatch
    case invalidCandidatePayload
    case invalidUTF8
}

public func encodeCandidates(_ candidates: [WireCandidate]) throws -> Data {
    guard candidates.count <= PipeMessage.maximumCandidateCount else {
        throw PipeProtocolError.invalidCandidatePayload
    }

    var payload = Data()
    payload.appendLittleEndian(UInt32(candidates.count))
    for candidate in candidates {
        try payload.appendLengthPrefixedUTF8(candidate.text)
        try payload.appendLengthPrefixedUTF8(candidate.annotation)
    }
    guard payload.count <= PipeMessage.maximumPayloadSize else {
        throw PipeProtocolError.payloadTooLarge
    }
    return payload
}

public func decodeCandidates(_ payload: Data) throws -> [WireCandidate] {
    guard payload.count >= MemoryLayout<UInt32>.size,
          payload.count <= PipeMessage.maximumPayloadSize else {
        throw PipeProtocolError.invalidCandidatePayload
    }

    let count = try payload.readUInt32LE(at: 0)
    guard count <= PipeMessage.maximumCandidateCount else {
        throw PipeProtocolError.invalidCandidatePayload
    }

    var offset = MemoryLayout<UInt32>.size
    var candidates: [WireCandidate] = []
    candidates.reserveCapacity(Int(count))
    for _ in 0..<count {
        let text = try payload.readLengthPrefixedUTF8(offset: &offset)
        let annotation = try payload.readLengthPrefixedUTF8(offset: &offset)
        candidates.append(WireCandidate(text: text, annotation: annotation))
    }
    guard offset == payload.count else {
        throw PipeProtocolError.lengthMismatch
    }
    return candidates
}

private extension Data {
    mutating func appendLittleEndian<T: FixedWidthInteger>(_ value: T) {
        var remaining = value.littleEndian
        for _ in 0..<MemoryLayout<T>.size {
            append(UInt8(truncatingIfNeeded: remaining))
            remaining >>= 8
        }
    }

    mutating func appendLengthPrefixedUTF8(_ text: String) throws {
        let bytes = Data(text.utf8)
        guard bytes.count <= PipeMessage.maximumPayloadSize else {
            throw PipeProtocolError.payloadTooLarge
        }
        appendLittleEndian(UInt32(bytes.count))
        append(bytes)
    }

    func readUInt16LE(at offset: Int) throws -> UInt16 {
        guard offset >= 0, count - offset >= 2 else {
            throw PipeProtocolError.invalidHeader
        }
        return UInt16(self[offset]) | (UInt16(self[offset + 1]) << 8)
    }

    func readUInt32LE(at offset: Int) throws -> UInt32 {
        guard offset >= 0, count - offset >= 4 else {
            throw PipeProtocolError.invalidCandidatePayload
        }
        return UInt32(self[offset]) |
            (UInt32(self[offset + 1]) << 8) |
            (UInt32(self[offset + 2]) << 16) |
            (UInt32(self[offset + 3]) << 24)
    }

    func readUInt64LE(at offset: Int) throws -> UInt64 {
        guard offset >= 0, count - offset >= 8 else {
            throw PipeProtocolError.invalidHeader
        }
        var value: UInt64 = 0
        for byte in 0..<8 {
            value |= UInt64(self[offset + byte]) << (byte * 8)
        }
        return value
    }

    func readLengthPrefixedUTF8(offset: inout Int) throws -> String {
        guard count - offset >= MemoryLayout<UInt32>.size else {
            throw PipeProtocolError.invalidCandidatePayload
        }
        let byteCount = Int(try readUInt32LE(at: offset))
        offset += MemoryLayout<UInt32>.size
        guard byteCount <= PipeMessage.maximumPayloadSize, count - offset >= byteCount else {
            throw PipeProtocolError.invalidCandidatePayload
        }
        let bytes = self[offset..<(offset + byteCount)]
        guard let value = String(data: bytes, encoding: .utf8) else {
            throw PipeProtocolError.invalidUTF8
        }
        offset += byteCount
        return value
    }
}
