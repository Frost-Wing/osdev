# Syscall implementation layout

`dispatcher.c` is the only syscall entry point. It contains the x86_64 and
`int 0x80` adapters plus `syscall_dispatch()`, where a syscall number is mapped
to its implementation.

Implementations are grouped by responsibility:

- `filesystem.c`: paths, file descriptors, directory iteration, and metadata.
- `network.c`: Linux socket compatibility and socket descriptor state.
- `process.c`: `execve`, fork/clone support, waiting, and futex compatibility.
- `system.c`: time, memory, signals, credentials, and miscellaneous kernel APIs.
- `internal.h`: private shared dependencies, cross-module declarations, and
  syscall-internal state. It is not a public kernel interface.

## Adding a syscall

1. Add the Linux syscall number and any ABI constants to `includes/syscalls.h`.
2. Put the implementation in the module that owns its subsystem. Declare it in
   `internal.h` only when the dispatcher or another module needs it.
3. Add the number-to-handler mapping in `dispatcher.c` (and the optional trace
   name in `names`).
4. Update `syscalls-doc.dox` when the compatibility behavior is user-visible.

The kernel Makefile discovers C sources recursively, so new `.c` files in this
directory are compiled automatically.
