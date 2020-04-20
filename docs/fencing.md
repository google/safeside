# Stopping the world

This doc describes how we implement a complete memory and speculation barrier on various architectures. These barriers serialize the thread of execution by preventing any later instructions from starting until all previous instructions and all memory operations have entirely finished.

The memory operations we want to wait for obviously include reads and writes, but also cache flushes.

We need a barrier like this in a few places:
- Preventing speculative execution from disturbing state we want to measure, e.g. the timing of a memory operation
- Waiting for a cache flush to finish so we know a later read will bring one element _back_ into the cache.
- Waiting for a change that changes the semantics of the whole instruction stream -- what ARM would call "context-changing operations" -- to take full effect before continuing even speculatively. An example is changing the alignment check (`AC`) bit in `EFLAGS` on x86.

## Implementing on x86

```assembly
MFENCE
LFENCE
```

The [load fence (`LFENCE`)](https://cpu.fyi/d/484#G5.136804) instruction serializes the instruction stream -- that is, it waits for all prior instructions to complete before allowing any later instructions to start.

However, on x86 a store is considered "complete" when it _starts_ (enters the store buffer), not when it's _visible_. And, as the name suggests, `LFENCE` doesn't take special care to wait for stores to reach memory. In order to serialize _all_ memory operations we also need a [memory fence (`MFENCE`)](https://cpu.fyi/d/484#G7.864843), which  [waits](https://cpu.fyi/d/749#G13.31870) for all prior reads, writes, and `CLFLUSH` and `CLFLUSHOPT` instructions to be entirely finished before completing.

The `LFENCE` has to come _after_ the `MFENCE` because only `LFENCE` serializes the instruction stream.

### Caveat: `LFENCE` on AMD

Technically, `LFENCE` is [_not_ documented as serializing the instruction stream](https://cpu.fyi/d/ad5#G9.3072402) on AMD processors. In early 2018 AMD described a model-specific register (MSR) that system software can set to make `LFENCE` serializing on all existing and future AMD processors. ([_Software techniques for managing speculation on AMD processors_](https://cpu.fyi/l/AxzQB), page 3, mitigation G-2.)

This MSR [has been set in Linux](https://git.io/JePgI) since kernel version 4.15.

### Other implementations

Another option for an unprivileged, fully-serializing instruction is [`CPUID`](https://cpu.fyi/d/484#G5.876260). One reason to avoid `CPUID` is that it can cause an exit to the hypervisor when running in a virtual machine. Hosts use this to control which CPU features are discoverable in the guest VM, sometimes to present a homogenous level of functionality when VMs can be migrated across different hosts. The net result is that `CPUID` can be very slow with high variance.

For the specific case of timing an instruction sequence, we could use [`RDTSCP`](https://cpu.fyi/d/484#G7.587245). Before reading from the timestamp counter, `RDTSCP` first waits for all previous instructions and loads from memory to finish. It does *not* stop later instructions from starting, so it can't be used to build a full barrier.

### Other references

- ["Serializing Instructions"](https://cpu.fyi/d/749#G13.8467) in the _Intel Software Developer's Manual_
- [_FLUSH+RELOAD: a High Resolution, Low Noise, L3 Cache Side-Channel Attack_](https://eprint.iacr.org/2013/448.pdf), specifically Figure 4 on page 5 and the accompanying explanation.

## Implementing on ARM64

```assembly
DSB SY
ISB
```

The [data synchronization barrier (`DSB`)](https://cpu.fyi/d/047#G9.10258412) instruction waits for all memory accesses and "cache maintenance instructions" to finish before completing, and prevents instructions later in program order from beginning *almost* any work until the `DSB` completes.

The two exceptions are:
- Instructions can be fetched and decoded
- Registers that are read "without causing side-effects"

These might seem innocuous, but empirically we've seen the second item extends to registers we _want_ to wait to read, e.g. the timestamp counter.

To build a complete barrier, we add the [instruction synchronization barrier (`ISB`)](https://cpu.fyi/d/047#G9.10257730) instruction. `ISB` ensures that all later instructions are fetched from memory and decoded after the `ISB` completes and that "context-changing operations" executed before the `ISB` are visible to instructions after.

## Implementing on PowerPC

```assembly
ISYNC
SYNC
```

The [synchronize (`SYNC`)](https://cpu.fyi/d/a48#G19.1034642) instruction waits for all preceding instructions to complete before any subsequent instructions are initiated. It also waits until _almost_ all preceding memory operations have completed, with the exception of those initiated by ["instruction cache block invalidate" (`ICBI`)](https://cpu.fyi/d/a48#G19.1020460), i.e. instruction cache flush.

To wait for these last accesses, we also issue the [instruction synchronize (`ISYNC`)](https://cpu.fyi/d/a48#G19.1020771) instruction. `ISYNC` has the same serializing effect on the instruction stream as `SYNC`, but doesn't enforce order of any memory accesses *except* those caused by a preceding `ICBI`.

There's no obvious reason the order of these two instructions should matter. [Linux uses `ISYNC; SYNC`](https://git.io/Je60x).

## Other implementation notes

The barrier must never be implemented as an indirect function call (e.g. `vtable` lookup or shared library export), since it's possible for the call itself to be mispredicted and for speculative execution to continue in an unintended direction.

It's safest for implementations to always be inlined into the caller.
