# X11Libre compatibility and security hardening

## Scope

This document defines the compatibility and security-hardening work required for HGBits/Hypr when used with the XLibre/X11Libre X server.

Hypr is an X11 client/window manager built on XCB. It does not link directly to the X server implementation. Therefore, XLibre server CVEs are normally fixed in the server and must not be copied into Hypr as if they were client vulnerabilities.

The objective is to:
1. preserve compatibility with the X11 protocol and XCB stack used by XLibre;
2. make Hypr robust against malformed or unexpected X11 replies/events;
3. remove local security weaknesses in the WM;
4. make the build suitable for modern XLibre/XCB environments.

## Current baseline

The repository is an old Hypr snapshot whose latest commit is from March 2024. The build requires XCB components including xcb, xcb-randr, xcb-ewmh, xcb-xinerama, xcb-cursor, xcb-keysyms, xcb-icccm, xcb-shape and xcb-util.

The implementation communicates with the display server through XCB rather than Xlib.

## Compatibility model

### No XLibre-specific API layer

Hypr should continue using XCB/X11 protocol interfaces. There is no reason to add an XLibre backend merely to run against XLibre.

Compatibility should be validated at three levels:

- Build: XCB headers and pkg-config dependencies resolve against the target distribution's XLibre stack.
- Protocol: X11, EWMH, ICCCM and RandR requests/replies used by Hypr remain valid.
- Runtime: malformed replies, missing extensions, allocation failures and server-side errors do not become undefined behavior.

## Priority map

| Area | File(s) | Priority | Reason |
|---|---|---:|---|
| IPC transport | src/windowManager.cpp, src/windowManager.hpp, src/ipc/ipc.cpp | Critical | Predictable shared files in /tmp allowed local interference and symlink attacks |
| XCB reply validation | src/utilities/XCBProps.cpp, src/ewmh/ewmh.cpp, src/windowManager.cpp | High | X11 properties are client-controlled input |
| XCB error handling | src/main.cpp, src/windowManager.cpp, event handlers | High | Asynchronous X errors are otherwise silently ignored |
| Unbounded property requests | src/ewmh/ewmh.cpp | High | UINT32_MAX is used as a property length |
| Empty-container handling | src/ewmh/ewmh.cpp | High | &windowsList[0] is undefined for an empty vector |
| VLA / size validation | src/ewmh/ewmh.cpp and other X11 paths | Medium | Runtime-sized stack allocations need explicit bounds |
| Build hardening | CMakeLists.txt | Medium | Current build forces /bin/g++ and lacks an explicit hardening policy |
| Resource lifetime | XCB cursor/context and replies | Medium | Cleanup should be deterministic |
| Xnamespace awareness | future integration tests | Medium | XLibre adds Xnamespace for client isolation |

## Security hardening

### 1. Replace /tmp/hypr IPC

The original IPC implementation created predictable regular files in /tmp/hypr:
- /tmp/hypr/hyprbarin
- /tmp/hypr/hyprbarout
- /tmp/hypr/hyprbarind
- /tmp/hypr/hyprbaroutd

Those files were created through system() and then used as the transport between the Hypr parent process and its built-in bar child. Because the paths were predictable and shared through /tmp, another local process could interfere with the endpoints or exploit filesystem semantics such as symlink replacement.

The hardening change replaces those file-based endpoints with an anonymous Unix-domain socketpair:
- socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, ...)
- one endpoint remains in the Hypr parent;
- the other endpoint remains in the bar child after fork();
- the single full-duplex stream carries both Hypr-to-bar and bar-to-Hypr messages;
- no IPC pathname is created in /tmp;
- no shell command or system() call is required to create the transport.

The existing IPC message format is retained. SOCK_STREAM does not preserve message boundaries, so HYPR_END_OF_FILE remains the application-level message terminator and the receiver buffers partial reads until a complete message is available.

The transport is established before fork(), and each process closes the peer endpoint it does not own. SOCK_CLOEXEC also prevents accidental inheritance across later exec operations.

Runtime validation on the hardened branch confirmed:
- Hypr starts normally from the LY X11 session;
- the built-in bar communicates normally;
- workspace/window updates continue to work;
- startup remains fast;
- the old hyprbar* files are not required for IPC.

This replaces the previous /tmp-based IPC threat model rather than moving the same named files into another directory.

### 2. Treat X11 properties as hostile input

Window properties are supplied by X11 clients. Every property read should validate, as appropriate:
- reply is non-null;
- property type matches the expected atom;
- format is exactly 8, 16 or 32 as expected;
- item count is within a sane maximum;
- calculated byte lengths do not overflow;
- the value pointer is non-null before dereferencing;
- strings are bounded before conversion to std::string;
- malformed data is ignored rather than becoming WM state.

Particular targets:
- getClassName()
- getRoleName()
- getWindowName()
- getWindowState()
- _NET_WM_STATE
- WM_TRANSIENT_FOR
- tray/XEmbed properties
- ICCCM size hints and protocols

### 3. Remove the unbounded UINT32_MAX property request

EWMH::checkTransient() requests WM_TRANSIENT_FOR using a length of UINT32_MAX.

The property is expected to contain a window identifier, so requesting gigabytes of data is unnecessary.

Use a small fixed maximum and validate the returned format/type before passing the reply to xcb_icccm_get_wm_transient_for_from_reply().

### 4. Fix XCB reply/error lifetime handling

For operations where failure affects WM state, prefer checked XCB requests and xcb_request_check().

Audit all paths for:
- leaked xcb_generic_error_t;
- replies freed on every path;
- errors checked before dereferencing replies;
- asynchronous errors that can leave internal state inconsistent.

xcb_disconnect() ends the connection lifetime. The current main.cpp checks xcb_connection_has_error() after disconnecting; that should be removed/reworked.

### 5. Remove undefined behavior around empty collections

EWMH::updateClientList() takes &windowsList[0] even when the vector may be empty.

Use windowsList.data() and explicitly handle an empty list.

The same principle applies to runtime-sized arrays used for EWMH desktop information.

### 6. Bound runtime-sized stack allocations

The current implementation uses C++ VLAs in several places even though VLAs are not standard C++.

Replace them with std::vector or fixed-size storage with explicit upper bounds, particularly where sizes derive from X11-controlled state.

### 7. Harden the build

CMakeLists.txt currently hard-codes:

    set(CMAKE_CXX_COMPILER "/bin/g++")

The modernization should:
- remove the hard-coded compiler;
- require a modern C++ standard explicitly;
- use compiler warnings consistently;
- enable PIE/RELRO/NOW/stack protector/fortify where supported;
- keep sanitizer builds available for CI;
- build against system XCB packages rather than assuming an Xorg-specific server ABI.

## XLibre security fixes that remain server-side

XLibre 25.x includes fixes inherited from X.Org and additional validation work, including:
- NULL-pointer checks in XI handlers;
- GLX integer-overflow and out-of-bounds fixes;
- XKB bounds checking;
- XFixes request-length validation;
- XTest stack bounds protection;
- glyph dimension/metric validation;
- byte-swapping fixes;
- allocation-failure handling;
- authentication randomness improvements.

These fixes belong to the X server. Hypr should consume the corrected server through normal X11/XCB protocol rather than duplicating server internals.

Hypr should nevertheless add regression tests for client-visible behavior affected by these protocol-validation changes.

## Compatibility test matrix

At minimum, CI should test Hypr against:
1. XLibre current stable 25.x;
2. X.Org Xserver current stable;
3. Xvfb/Xephyr where useful for protocol-level tests;
4. an XCB implementation from the target distribution.

Runtime tests should cover:
- startup and shutdown;
- RandR monitor discovery;
- monitor hotplug;
- EWMH desktop creation/update;
- normal/floating windows;
- transient windows;
- tray/XEmbed;
- malformed window properties;
- missing optional X extensions;
- X server error paths.

Security tests should include:
- attacker-controlled properties with invalid type/format;
- truncated property replies;
- oversized property lengths;
- invalid string payloads;
- repeated RandR notifications;
- IPC endpoint interference attempts;
- verification that the obsolete /tmp/hypr/hyprbar* endpoints are not created.

## Acceptance criteria

Hypr is considered XLibre-compatible when:
- it builds without Xorg-private headers or server-internal ABI dependencies;
- it runs on XLibre using the standard XCB stack;
- no XLibre-specific compatibility shim is required for normal X11/EWMH/ICCCM/RandR operation;
- malformed X11 client properties do not crash or corrupt Hypr;
- IPC endpoints cannot be hijacked through predictable filesystem paths;
- the Hypr/bar IPC transport uses process-local socketpair endpoints rather than named /tmp files;
- XCB errors are handled deterministically;
- CI exercises both XLibre and X.Org server implementations.

## References

- XLibre Xserver: https://github.com/X11Libre/xserver
- XLibre releases: https://github.com/X11Libre/xserver/releases
- XLibre compatibility matrix: https://github.com/X11Libre/xserver/wiki/Are-We-XLibre-Yet%3F
- HGBits/Hypr: https://github.com/HGBits/Hypr
