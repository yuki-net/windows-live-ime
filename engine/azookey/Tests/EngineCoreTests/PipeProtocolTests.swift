import XCTest
@testable import EngineCore

final class PipeProtocolTests: XCTestCase {
    func testFrameMatchesWireHeaderAndRoundTrips() throws {
        let input = Data([0xE3, 0x81, 0x8B, 0xE3, 0x81, 0xAA])
        let message = PipeMessage(
            kind: .convert,
            requestID: 100,
            generationID: 102,
            payload: input
        )

        let encoded = try message.encoded()
        XCTAssertEqual(Array(encoded.prefix(4)), Array("WIME".utf8))
        XCTAssertEqual(try PipeMessage.payloadSize(fromHeader: Data(encoded.prefix(32))), input.count)
        XCTAssertEqual(try PipeMessage.decode(encoded), message)
    }

    func testCandidatePayloadRoundTripsJapaneseText() throws {
        let candidates = [
            WireCandidate(text: "仮名", annotation: "かな"),
            WireCandidate(text: "漢字", annotation: "かんじ"),
        ]
        let encoded = try encodeCandidates(candidates)
        XCTAssertEqual(try decodeCandidates(encoded), candidates)
    }

    func testInvalidVersionAndLengthAreRejected() throws {
        let encoded = try PipeMessage(kind: .ping, requestID: 1, generationID: 1).encoded()
        var wrongVersion = encoded
        wrongVersion[4] = 2
        XCTAssertThrowsError(try PipeMessage.decode(wrongVersion))

        var trailingByte = encoded
        trailingByte.append(0)
        XCTAssertThrowsError(try PipeMessage.decode(trailingByte))
    }
}
