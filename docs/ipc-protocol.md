# Named Pipe protocol v1

Pipe name: `\\.\pipe\windows-live-ime`

All integer fields are little-endian. The fixed header is 32 bytes, followed by the declared payload. Maximum payload is 1 MiB.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `WIME` |
| 4 | 2 | protocol version (`1`) |
| 6 | 2 | message kind |
| 8 | 4 | payload byte count |
| 12 | 8 | request ID |
| 20 | 8 | generation ID |
| 28 | 4 | status code |

Message kinds are `Ping=1`, `Pong=2`, `Convert=3`, `ConvertResult=4`, and `Error=5`. A ping has an empty payload; the host returns a pong with the same request and generation IDs. A convert payload is the input text as UTF-8. A convert result payload starts with a 32-bit candidate count, then repeats a 32-bit UTF-8 byte count and text bytes for candidate text and annotation.

The C++ protocol tests and Swift protocol tests cover Japanese candidate text, IDs, version, and length handling. The C++ Named Pipe client uses a caller-supplied deadline for connect, write, and read operations. A timeout cancels pending I/O and reports failure to the caller.
