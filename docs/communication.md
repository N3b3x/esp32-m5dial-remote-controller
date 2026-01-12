# Communication Protocol

The system uses the ESP-NOW protocol for low-latency, connectionless wireless communication between the Remote Controller (M5Dial) and the Fatigue Test Unit.

## Protocol Stack

*   **Transport**: ESP-NOW (vendor-specific WiFi frame).
*   **Security**: Optional Pairing/Encryption (placeholder in current implementation).
*   **Packet Structure**: Custom binary protocol with framing and CRC16.

## Packet Format

All messages are encapsulated in the `EspNowPacket` structure:

| Field | Size | Description |
| :--- | :--- | :--- |
| `Sync` | 1 byte | Synchronization byte (`0xAA`) |
| `Version` | 1 byte | Protocol version (`1`) |
| `DeviceID` | 1 byte | Source Device ID |
| `Type` | 1 byte | Message Type ID (see below) |
| `ID` | 1 byte | Sequence ID / Command ID |
| `Len` | 1 byte | Payload Length |
| `Payload` | N bytes | Message Data (Max 200 bytes) |
| `CRC` | 2 bytes | CRC16-CCITT of the header and payload |

## Message Types

Defined in `espnow_protocol.hpp`:

| ID | Name | Description |
| :--- | :--- | :--- |
| 1 | `DeviceDiscovery` | Broadcast to find available units. |
| 2 | `DeviceInfo` | Response from unit with capabilities. |
| 3 | `ConfigRequest` | Request current settings from a unit. |
| 4 | `ConfigResponse` | Unit sends its current configuration. |
| 5 | `ConfigSet` | Remote pushes new configuration to unit. |
| 6 | `ConfigAck` | Acknowledgment of config change. |
| 7 | `Command` | Runtime command (Start, Stop, Pause). |
| 8 | `CommandAck` | Acknowledgment of command. |
| 9 | `StatusUpdate` | Periodic status (cycles, state, error). |
| 13 | `BoundsResult` | Result of bounds finding procedure. |

## Application Payloads

Defined in `fatigue_protocol.hpp`:

### Configuration Payload (`ConfigPayload`)
Used in `ConfigSet` and `ConfigResponse`.

*   **Base Fields**:
    *   `cycle_amount` (uint32): Target cycles (0 = infinite).
    *   `oscillation_vmax_rpm` (float): Max velocity.
    *   `oscillation_amax_rev_s2` (float): Max acceleration.
    *   `dwell_time_ms` (uint32): Wait time at endpoints.
    *   `bounds_method` (uint8): 0=StallGuard, 1=Encoder.
*   **Extended Fields**:
    *   Bounds search parameters (velocities, currents, stall factors).
    *   `stallguard_sgt` (int8): StallGuard threshold.

### Command IDs
Used in `Command` message type.

1.  `Start`: Begin fatigue test.
2.  `Pause`: Pause test.
3.  `Resume`: Resume test.
4.  `Stop`: Stop test.
5.  `RunBoundsFinding`: Initiate bounds finding procedure.

### Status Payload (`StatusPayload`)
Sent periodically by the test unit.

*   `cycle_number` (uint32): Current cycle count.
*   `state` (uint8): Idle, Running, Paused, Completed, Error.
*   `err_code` (uint8): Error code if state is Error.
*   `bounds_valid` (uint8): Validity of current mechanical bounds.

## Pairing & Security
The current implementation supports a pairing handshake (Request, Response, Confirm) to establish a trusted relationship, but typically operates in an open mode where the remote targets a specific test unit MAC address defined in `config.hpp`.
