# AGENTS.md

## Repository purpose

Hypr is a legacy X11 window manager written in modern C++ and using XCB. This fork is being maintained with a specific focus on security hardening and compatibility with XLibre/X11Libre.

The repository is an X11 client/window manager. Do not treat Xorg/XLibre server internals as part of this codebase.

## Read first

Before modifying code, read:

1. `README.md`
2. `docs/X11Libre-SECURITY.md`
3. the relevant source/header files
4. existing build/test configuration

## Core rules for AI agents

### Scope discipline

- Make the smallest patch that solves the identified problem.
- Do not perform unrelated refactors.
- Preserve existing behavior unless the change is explicitly security-related.
- Do not rewrite the WM architecture merely to modernize style.
- Do not introduce an XLibre-specific backend when standard XCB/X11 interfaces are sufficient.
- Do not copy X server implementation code into Hypr.

### Security

Treat all X11 client-controlled data as untrusted.

For XCB replies and X11 properties:

- check reply pointers before dereferencing;
- verify property type and format;
- validate item counts and byte lengths;
- prevent integer overflow in size calculations;
- impose reasonable limits on externally controlled allocations;
- handle malformed input by rejecting/ignoring it safely;
- free XCB replies and errors on every path;
- use checked XCB requests where failure can affect WM state.

Never use `system()` for filesystem setup when a direct syscall/library operation is sufficient.

Do not create security-sensitive IPC endpoints in shared `/tmp` paths. Prefer `$XDG_RUNTIME_DIR` with a private directory and restrictive permissions.

Do not follow attacker-controlled symlinks when opening IPC or runtime files.

### XLibre compatibility

Hypr should communicate through standard XCB/X11, EWMH, ICCCM and RandR interfaces.

XLibre server CVEs normally belong to the X server. Only add a Hypr patch when there is a demonstrable client-side problem, protocol compatibility issue, or unsafe assumption exposed by the server behavior.

When changing protocol handling, test against both XLibre and X.Org when practical.

### C++

- Follow the existing code style unless it conflicts with correctness or security.
- Avoid non-standard C++ features such as VLAs.
- Prefer RAII and standard containers for owned resources.
- Avoid raw ownership when a safe existing abstraction is available.
- Keep conversions and integer boundaries explicit.
- Do not silence compiler warnings merely to make a build pass.

### Build system

- Do not hard-code a compiler path.
- Preserve distro/toolchain selection.
- Keep normal builds portable.
- Add hardening flags conditionally when supported.
- Keep sanitizer configurations usable for security testing.

## Review checklist

Before declaring a security patch complete:

- Can an X11 client control the input being parsed?
- Can a malformed reply cause a null dereference?
- Can a count/length overflow a calculation?
- Can an attacker force an unbounded allocation or stack use?
- Are all XCB replies/errors released?
- Can a filesystem path be replaced by a symlink?
- Is a shared temporary path used for security-sensitive IPC?
- Does the patch preserve normal X11/EWMH/ICCCM behavior?
- Was unrelated behavior changed?
- Can the change be tested without a real desktop session?

## Testing expectations

At minimum, build the project after source changes.

For X11 protocol changes, test:

- startup/shutdown;
- window creation and destruction;
- floating and tiled windows;
- EWMH client list/state;
- transient windows;
- RandR monitor detection;
- malformed X11 properties;
- missing optional extensions;
- XCB server errors.

For IPC changes, test:

- runtime directory creation;
- restrictive permissions;
- pre-existing path handling;
- symlink replacement attempts;
- normal bar communication.

Prefer deterministic tests over tests that depend on a user's desktop session.

## Patch documentation

Security-sensitive commits should explain:

1. the affected code path;
2. the attacker-controlled input or unsafe resource;
3. why the previous behavior was unsafe;
4. the mitigation;
5. how it was tested.

Do not claim an XLibre CVE is fixed in Hypr unless the vulnerability actually exists in Hypr.

## Current security work

The current hardening roadmap is documented in:

`docs/X11Libre-SECURITY.md`

The highest-priority items are:

1. secure the bar IPC runtime files;
2. validate XCB/X11 properties and replies;
3. bound `WM_TRANSIENT_FOR` reads;
4. eliminate undefined behavior and VLAs in X11 paths;
5. audit XCB error/reply lifetimes;
6. modernize build hardening;
7. add XLibre/X.Org regression coverage.

## Agent workflow

For every task:

1. Inspect the smallest relevant set of files.
2. Establish the existing behavior before changing it.
3. Identify security boundaries and attacker-controlled data.
4. Make one coherent patch.
5. Build and run relevant tests.
6. Review the diff for unrelated changes.
7. Update documentation when behavior or security assumptions change.

If the requested change conflicts with these rules, prefer the safer and narrower implementation and document the reason.
