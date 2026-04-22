# Mini-Redis

A from-scratch implementation of a Redis-like in-memory key-value store written in C++. Built as a learning project to explore non-blocking sockets, event loops, custom binary protocols, and hashtable internals.

## Features

- TCP server listening on port `1234`
- Non-blocking I/O using `poll()`-based event loop
- Length-prefixed binary request/response protocol with pipelining support
- Custom hashtable with **progressive rehashing** (no stop-the-world resize)
- Tagged serialization format (nil, error, string, int, double, array)
- Supports multiple concurrent client connections

## Supported Commands

| Command           | Description                              |
|-------------------|------------------------------------------|
| `get <key>`       | Retrieve the value for a key             |
| `set <key> <val>` | Store a key-value pair                   |
| `del <key>`       | Delete a key, returns 1 if removed       |
| `keys`            | List all keys in the store               |

## Build

```bash
g++ -Wall -Wextra -O2 -std=c++17 servercpp.cpp hashtable.cpp -o server
```

## Run

```bash
./server
```

The server binds to `0.0.0.0:1234`. Connect with any TCP client that speaks the wire protocol.

## Wire Protocol

### Request

Each request is a length-prefixed frame containing a string-array command:

```
+-----+------+-----+------+-----+------+-----+-----+------+
| len | nstr | len | str1 | len | str2 | ... | len | strn |
+-----+------+-----+------+-----+------+-----+-----+------+
   4    4      4    ...     4    ...           4    ...
```

- `len` (4 bytes): total payload size, little-endian `uint32`
- `nstr` (4 bytes): number of strings in the command
- Each string is a 4-byte length followed by raw bytes

### Response

Responses are length-prefixed frames containing a single tagged value:

| Tag        | Value | Payload                                    |
|------------|-------|--------------------------------------------|
| `TAG_NIL`  | `0`   | (none)                                     |
| `TAG_ERR`  | `1`   | `uint32` code + `uint32` len + bytes       |
| `TAG_STR`  | `2`   | `uint32` len + bytes                       |
| `TAG_INT`  | `3`   | `int64`                                    |
| `TAG_DBL`  | `4`   | `double` (8 bytes)                         |
| `TAG_ARR`  | `5`   | `uint32` n + n encoded values              |

## Project Structure

```
.
├── servercpp.cpp   # Event loop, protocol parsing, command dispatch
├── hashtable.h     # HMap / HTab / HNode interface
├── hashtable.cpp   # Chained hashtable with progressive rehashing
└── server.c        # Earlier C-only single-client version
```

## Implementation Notes

### Event Loop

`poll()` is used to multiplex the listening socket and all client connections. Each `Conn` carries an `incoming` and `outgoing` byte buffer plus `want_read` / `want_write` / `want_close` intent flags that the loop translates into `POLLIN` / `POLLOUT` requests.

### Pipelining

`handle_read()` calls `try_one_request()` in a loop so that multiple requests delivered in a single `read()` are all processed before yielding back to the event loop.

### Progressive Rehashing

When the load factor exceeds `k_max_load_factor` (8), the hashtable allocates a new larger table without freeing the old one. Subsequent operations migrate up to `k_rehashing_work` (128) entries each, amortizing the resize cost so no single operation pays for the full rehash.

## Limits

- Maximum message size: 32 MiB
- Maximum command arguments: 200,000
- No persistence — data lives only in memory while the server runs
- No authentication, no TLS

## Status

Work in progress. This is a learning exercise, not a production server.
