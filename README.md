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
| 1 | System call (`procinfo`) | `feat/sys-procinfo` | `lab1-syscall` | Done |
| 2 | Priority scheduler (`setpriority`) | `feat/priority-scheduler` | `lab2-scheduler` | Done |
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

## Lab 1: `procinfo` System Call

### What I changed
- Added `procinfo(int pid, struct procinfo *info)`, which returns a process's
  pid, parent pid, state, memory size, and name. Returns 0 on success, -1 if
  the pid does not exist or the user pointer is invalid.
- `kernel/procinfo.h`: struct shared by kernel and user code.
- `kernel/proc.c`: `procinfo()` scans `proc[]`, holding `wait_lock` and then
  `p->lock` (same order as `wait()`), and copies fields into a kernel-stack
  buffer.
- `kernel/sysproc.c`: `sys_procinfo()` reads arguments with `argint`/`argaddr`
  and calls `copyout` only after all locks are released.
- Wired up `kernel/syscall.h`, the table in `kernel/syscall.c`,
  `user/usys.pl`, and `user/user.h`.
- Added the test program `user/procinfotest.c`.

### How to test
```
make qemu
$ procinfotest
pid=4 ppid=2 state=run sz=16384 name=procinfotest
procinfotest: OK
```
The test also checks that a nonexistent pid and an unmapped user pointer
both return -1.

### What I learned
- The user-to-kernel path: `ecall` -> `uservec` (trampoline, mapped at the
  same VA in both page tables) -> `usertrap()` -> `syscall()` -> `sret`.
- The syscall table uses designated initializers. A missing entry shows up
  as `unknown sys call N` because the unset slot is NULL.
- Kernel code must never dereference user pointers directly; `copyout`
  walks the user page table and fails safely.
- `parent` is protected by `wait_lock`, not `p->lock`, and locks must be
  taken in a fixed order to avoid deadlock.
- Holding a spinlock across `copyout` is unsafe because it can fault and
  sleep, so the data is copied to a local buffer first.

## Lab 2: Priority Scheduler

### What I changed
- Added `priority` (0 = highest, 9 = lowest, default 5) to `struct proc`,
  protected by `p->lock`; children inherit the parent's priority in `kfork()`.
- Replaced the round-robin loop in `scheduler()` with a two-pass selection:
  pass 1 scans `proc[]` (each slot under its own lock) for the RUNNABLE
  process with the smallest priority number; pass 2 locks the winner and
  re-checks `state == RUNNABLE` before `swtch()`.
- Equal priorities still take turns: each scan starts after the slot that
  ran last on this CPU (`last`, a local variable in `scheduler()`).
- Added `setpriority(pid, prio)` (returns 0, or -1 for a bad pid or an
  out-of-range priority).
- Added `user/schedtest.c`: three CPU-bound children with priorities 1, 5, 9.
- `GNUmakefile` now forwards `CPUS` to the container, and the Docker image
  installs `bc` (needed by the Makefile's QEMU version check).

### How to test
```
make qemu CPUS=1
$ schedtest
```

| Scheduler | child 0 (prio 1) | child 1 (prio 5) | child 2 (prio 9) |
|---|---|---|---|
| Round robin, equal priority | 36 ticks | 36 ticks | 36 ticks |
| Priority scheduler | 12 ticks | 26 ticks | 38 ticks |

Run on a single CPU so the children compete for it. To check for races, boot
with the default 3 CPUs and run `usertests -q`; all tests pass. The
`usertrap(): unexpected scause` lines in its output are expected, because
some tests deliberately touch invalid addresses.

### What I learned
- Candidates are chosen without holding a lock across the whole scan, so the
  choice can be stale; the re-check under the lock in pass 2 is what prevents
  two cores from running the same process.
- `p->lock` is held across `swtch()` and released by the other side (lock
  handoff), which makes the state change and the context switch atomic.
- Strict priority scheduling can starve low-priority processes, and each
  pick costs O(NPROC). Aging or per-priority run queues would address this.
