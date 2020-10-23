# Mitigating ret2spec

This doc explains ret2spec attacks and how they are mitigated between different execution modes. In particular, we explain the subtleties of how the Linux kernel is protected from ret2spec attacks originating from user processes.

## Background: anatomy of a ret2spec attack

Function calls on x86 are built on two instructions: `CALL` and `RET`. `CALL` pushes the address of the next instruction -- the _return address_ -- onto the stack, then jumps to the start of the function being called. When that function is done, it uses `RET` to pop the return address off the stack and jump back to it.

It can take a while for a `RET` to fully resolve the return address -- for example, if the address has been evicted from cache. To avoid stalling out-of-order execution, modern processors use a _return stack buffer_ (RSB) to remember return addresses. When a `CALL` executes, the return address is pushed onto the RSB; later, when we get to a `RET`, the top entry is popped off the RSB and used as the predicted return address.

This optimization assumes, very reasonably, that `CALL`s and `RET`s are paired and return addresses on the stack aren't modified between the `CALL` and `RET`. That said, the return address _is_ still read from memory and eventually compared with the RSB's prediction. If they don't agree, the results of the mis-speculated instructions after the `RET` are thrown away and execution restarts at the correct target.

Entries in the RSB can persist even after a context switch or privilege change. If the code that runs after the switch executes more `RET`s than `CALL`s, it may consume RSB entries it did not create. Those entries can send speculative execution to unexpected places, which might in turn leak information through side-channels.

An attempt by one security domain to control speculative execution in another domain via adversarial RSB entries is known as a _ret2spec_ or _SpectreRSB_ attack. For more details, see [_ret2spec: Speculative Execution Using Return Stack Buffers_](https://arxiv.org/abs/1807.10364) and [_Spectre Returns! Speculation Attacks using the Return Stack Buffer_](https://arxiv.org/abs/1807.07940).

## Linux mitigations for ret2spec

This discussion only covers mitigations on `x86`.

There are four places where we need to worry about code consuming RSB entries created in another context:

1. User process to user process
2. User process to kernel
3. Virtual machine to virtual machine
4. Virtual machine to hypervisor

The most straightforward mitigation for ret2spec is to fill or "stuff" the RSB with benign entries. The Linux kernel has a [`FILL_RETURN_BUFFER` assembler macro](https://github.com/torvalds/linux/blob/08bf1a27c4c354b853fd81a79e953525bbcc8506/arch/x86/include/asm/nospec-branch.h#L105) that accomplishes this, which is used on [context switches](https://github.com/torvalds/linux/blob/2646fb032f511862312ec8eb7f774aaededf310d/arch/x86/entry/entry_64.S#L253) and VM exits ([Intel VT-x](https://github.com/torvalds/linux/blob/921d2597abfc05e303f08baa6ead8f9ab8a723e1/arch/x86/kvm/vmx/vmenter.S#L83), [AMD SVM](https://github.com/torvalds/linux/blob/921d2597abfc05e303f08baa6ead8f9ab8a723e1/arch/x86/kvm/svm/vmenter.S#L107)).

Filling the RSB as part of every context switch prevents user processes from attacking each other. The RSB fill immediately after VM exit prevents that VM from attacking other VMs or the hypervisor.

This leaves one last attack vector: user to kernel. The Linux kernel does **not** fill the RSB on kernel entry. So, how is the kernel protected from user processes?

## An appealing but incomplete answer

A ret2spec attack is only possible when code consumes an RSB entry it didn't create -- when it executes a `RET` without an earlier paired `CALL`. Maybe that can't happen when we enter the kernel?

Context switches and VM exits cause execution to resume at an arbitrary depth in a reactivated call stack, which means we'll encounter unpaired `RET`s and we need to worry about ret2spec. But when we enter the kernel to handle an interrupt or system call, code starts executing at a known entry point. It might seem like `CALL`s and `RET`s should all be properly paired after that -- at least, until we exit the kernel or hit a context switch.

This doesn't **necessarily** hold true. For example, an especially clever system call handler might decide to implement a `CALL` as a `PUSH` and `JMP` -- maybe to cause the function to return somewhere other than the syscall handler. The resulting code works architecturally, but might speculatively consume an RSB entry populated by the user process.

Somewhat less artificially, it is possible for an unpaired `RET` to be reached _speculatively_. Consider a system call implementation that chooses a "back half" handler and then jumps to it:

```asm
; Given an operation index, look up the handler in an array and run it
SyscallHander(int):
 movsxd rax,edi                     ; rax = first argument
 jmp    QWORD PTR [rax*8+0x404040]  ; jump to entry in handler array

; Some function that happens to follow in memory and ends in `ret`
AnotherFunction():
 mov    eax,0x5
 ret
```

The _branch target buffer_ (BTB) is used to predict the target of the indirect jump. On a BTB miss, the predictor on some x86 CPUs will choose to continue speculative execution at the next instruction -- the idea being, while that code may not run _next_, it will run _soon_, and we might as well "fall through" and start speculatively executing memory loads that will be useful later.

Here, that effectively means falling through to the next function and executing a (now-mismatched) `RET`, creating a ret2spec vulnerability.

## Mitigations that help, just not here

Maybe user to kernel ret2spec attacks are already covered by another mitigation? After all, Linux already enables _indirect branch restricted speculation_ (IBRS), which mitigates branch target injection (aka Spectre Variant 2) attacks against the kernel.

Checking the CPU documentation, though, we see that IBRS is _explicitly_ not a defense against ret2spec.

[Intel](https://web.archive.org/web/20200805193149/https://software.intel.com/security-software-guidance/insights/deep-dive-indirect-branch-restricted-speculation):
> Setting `IA32_SPEC_CTRL.IBRS` to 1 does not suffice to prevent the predicted target of a near return from using an RSB entry created in a less privileged predictor mode. Software can avoid this by using a Return Stack Buffer (RSB) overwrite sequence following a transition to a more privileged predictor mode.

[AMD](https://web.archive.org/web/20200805195556/https://developer.amd.com/wp-content/resources/Architecture_Guidelines_Update_Indirect_Branch_Control.pdf):
> Clearing out the return stack buffer maybe required on the transition from `CPL3` to `CPL0`, even if the OS has SMEP enabled.

## The real answer: SMEP

Right after the snippet above, the Intel docs continue:

> [...] It is not necessary to use such a sequence following a transition from user mode to supervisor mode if supervisor-mode execution prevention (SMEP) is enabled. SMEP prevents execution of code on user mode pages, even speculatively, when in supervisor mode. User mode code can only insert its own return addresses into the RSB; not the return addresses of targets on supervisor mode code pages.

That's pretty straightforward, but let's quickly break it down. With SMEP enabled, the kernel doesn't need to worry about consuming an RSB entry created by a user process since:
1. Any RSB entries created by user mode must point to user memory (because `CALL` can only insert the address of the next instruction into the RSB)
2. SMEP will prevent the kernel from speculating into user memory

The Linux kernel [enables SMEP](https://github.com/torvalds/linux/blob/a09b1d78505eb9fe27597a5174c61a7c66253fe8/arch/x86/kernel/cpu/common.c#L1557-L1560), so it's protected from ret2spec.

### Addressing AMD's caveat

Intel says SMEP makes an RSB fill unnecessary. But above we saw AMD's docs say an RSB fill might be necessary on kernel entry "even if the OS has SMEP enabled".

Does that mean AMD processors act differently and need a different mitigation? Thankfully, no.

AMD is just being incredibly conservative in the assumptions they make about OS behavior.

Remember we said `CALL` instructions can only push their real return address onto the RSB, so user processes can only push user addresses. But what if a user address somehow _became_ a kernel address?

What if another thread takes the page that address falls on, unmaps it from the user process, and repurposes it for kernel code? In that very convoluted case, SMEP would *not* prevent a later `RET` from predicting a return to the address pushed onto the RSB by the user process.

For SMEP to be an effective mitigation against ret2spec, any address that _can_ be pushed by a `CALL` in user mode must **never** become an address where kernel code is mapped. Since `CALL` pushes the address of the _next_ instruction, that means any page that used to have user code mapped immediately _before_ must also stay off-limits to kernel code.

Conveniently, Linux [statically partitions the virtual address space](https://www.kernel.org/doc/Documentation/x86/x86_64/mm.txt) and has separate, safely-distanced ranges where user and kernel code can be mapped. The warning from AMD's documentation doesn't apply.

## Another real answer: NX

Intel's docs say one more thing about mitigating ret2spec:
> On parts without SMEP where separate page tables are used for the OS and applications, the OS page tables can map user code as no-execute. The processor will not speculatively execute instructions from a translation marked no-execute.

On Intel CPUs that are vulnerable to Meltdown, Linux enables [kernel page table isolation (KPTI)](https://www.kernel.org/doc/html/latest/x86/pti.html) which implements exactly this configuration:
> Although _complete_, the user portion of the kernel page tables is crippled by setting the NX bit in the top level. This ensures that any missed kernel->user CR3 switch will immediately crash userspace upon executing its first instruction.

To check if a CPU is vulnerable to Meltdown (aka _rogue data cache load_ or RDCL), Linux uses a [list of "safe" CPUs and looks at whether `CPUID` enumerates `RDCL_NO`](https://github.com/torvalds/linux/blob/a09b1d78505eb9fe27597a5174c61a7c66253fe8/arch/x86/kernel/cpu/common.c#L1191-L1196). A quick glance suggests any Intel CPU that's too old to support SMEP is _definitely_ vulnerable to Meltdown and will have KPTI enabled, meaning ret2spec is mitigated on newer _and_ older Intel CPUs.

### Not on AMD, though

AMD also confirms their processors won't speculate into no-execute memory. However, AMD CPUs are [not vulnerable to Meltdown](https://github.com/torvalds/linux/blob/a09b1d78505eb9fe27597a5174c61a7c66253fe8/arch/x86/kernel/cpu/common.c#L1078-L1085), so Linux won't enable KPTI while running on an AMD CPU.

Therefore, on AMD CPUs that don't support SMEP -- which seems to be anything earlier than their Excavator microarchitecture [introduced in 2015](https://web.archive.org/web/20150607022057/https://www.extremetech.com/mobile/207229-207229) -- the Linux kernel won't use SMEP or NX to defend against ret2spec attacks from user processes.

## Wrapping up: why all the trouble?

This made for a pretty long and windy exploration. Why didn't Linux just go with the obvious mitigation and fill the RSB on kernel entry?

The answer is performance. The RSB is big enough that we can enter and leave the kernel without overwriting all of its entries. When we return to user code, the next `RET` can be successfully predicted. That would not be the case if we decided to clear the RSB on every entry to the kernel.

Finally, user to kernel is the *only* transition where makes sense to leave the RSB intact, because those two contexts coexist inside the same virtual address space. Compare that to a context switch or VM exit, where the contents of the virtual address space change and the old RSB entries almost certainly point to code that's no longer mapped.
