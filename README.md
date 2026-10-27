# xv6-riscv-labs

Hands-on kernel work on [xv6-riscv](https://github.com/mit-pdos/xv6-riscv):
a reproducible Docker build environment, followed by a series of
self-defined labs (system calls, scheduling, memory, file system,
synchronization). Each lab lives on its own branch and is merged into
`main` with a tag.

The original xv6 notice is kept in [`README`](README).

## Labs

| # | Topic | Branch | Tag | Status |
|---|---|---|---|---|
| 0 | Environment (Docker, QEMU, gdb) | `chore/docker-env`, `chore/gdb-setup` | `lab0-env` | Done |
| 1 | System call | `feat/sys-procinfo` | - | Planned |
| 2 | Priority scheduler | `feat/priority-scheduler` | - | Planned |
| 3 | Memory (lazy alloc / COW fork) | `feat/cow-fork` | - | Planned |
| 4 | File system (double indirect blocks / symlinks) | `feat/fs-double-indirect` | - | Planned |
| 5 | Synchronization / lock contention | `feat/lock-contention-stats` | - | Planned |

## Quick start

Requires only Docker. Works the same on Intel (x86_64) and Apple Silicon (arm64).

```
make image      # build the xv6-dev image once
make qemu       # build and boot xv6; exit with Ctrl-a x
make qemu-gdb   # terminal A: QEMU waits for a debugger
make gdb        # terminal B: attach gdb-multiarch
```

Every target other than `image` and `gdb` is forwarded to the upstream
`Makefile` inside the container, so `make clean`, `make CPUS=1 qemu`, etc.
work as usual.

## Lab 0: Reproducible Build & Debug Environment

### What I changed
- Added a `Dockerfile` (Ubuntu 24.04) with `gcc-riscv64-linux-gnu`,
  `qemu-system-misc`, and `gdb-multiarch`, so the same toolchain runs on
  both Intel (x86_64) and Apple Silicon (arm64) Macs.
- Added a `GNUmakefile` wrapper that forwards every target to the original
  `Makefile` inside the container. The upstream `Makefile` is untouched, so
  the lab diffs stay free of environment code. `make gdb` attaches gdb to
  the running QEMU container.
- Added `.gdbinit.tmpl-riscv` (from upstream), used by `make qemu-gdb` to
  generate `.gdbinit`.

### How to test
```
make qemu
```
Expect `xv6 kernel is booting`, `init: starting sh`, and a `$` prompt.
Try `ls` and `echo hi`.

```
make qemu-gdb      # terminal A
make gdb           # terminal B
(gdb) x/5i $pc     # QEMU reset ROM at 0x1000
(gdb) b _entry
(gdb) c            # stops at _entry (0x80000000)
(gdb) disable 1
(gdb) b main
(gdb) c            # stops in main() on each core
```

### What I learned
- Boot path: QEMU reset ROM (`0x1000`) -> `_entry` at `0x80000000`
  (`kernel/entry.S`, per-hart stack) -> `start()` in machine mode
  (`kernel/start.c`) -> `mret` -> `main()` in supervisor mode.
- With `-smp 3`, all harts run `_entry`. Only hart 0 initializes the
  kernel; the others wait on a `started` flag and then set up their own
  per-hart state.
- The host-compiled `mkfs` and the cross-compiled kernel use different
  compilers, which is why the container needs both `build-essential` and
  `gcc-riscv64-linux-gnu`.
- gdb must run in the same container as QEMU because `.gdbinit` connects
  to `127.0.0.1:25000` (port derived from uid 0 inside the container).
