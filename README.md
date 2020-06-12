# SafeSide

[![Travis build status](https://travis-ci.org/google/safeside.svg?branch=main)](https://travis-ci.org/google/safeside)

SafeSide is a project to understand and mitigate *software-observable side-channels*: information leaks between software domains caused by implementation details _outside_ the software abstraction.

Unlike other [side-channel attacks](https://en.wikipedia.org/wiki/Side-channel_attack) -- e.g. measuring power use or electromagnetic emissions -- software-observable side-channels don't require physical access or proximity.

Our early focus is on [transient execution attacks](https://arxiv.org/abs/1811.05441) and leaks from software cryptography implementations.

## What's here?

This repository provides a home for:

- **Practical demonstrations**\
  Robust, portable examples that leak data through different side-channels under real-world conditions.

  For more on building and running our examples, see [the demos README](demos/README.md).

- **Documentation**\
  References to research that describes what causes side-channels and how they behave.

  Docs are [here](docs/README.md).

- **Mitigation development**\
  Ideas and prototypes for how to find and stop side-channel leaks.

## Status

This project is new and under active development. To start, we have a few examples of Spectre side-channel leaks and a collection of links we've found useful.

For more information on our plans and priorities, see our [Roadmap](#Roadmap).

## Non-goals

This project is focused on defense. While we enthusiastically believe in the value of adversarial testing (e.g. red teaming) for securing software systems, this repository is *not* intended to advance the state of the art for attackers.

To that end, we have some ground rules:

- **No nonpublic attacks.** This isn't the place to research new side-channels, discuss any embargoed or otherwise undisclosed vulnerabilities, or try to bypass currently-effective mitigations.

- **No exploits that leak interesting data.** We want to show side-channels in action, but we will only leak synthetic data that we put there to be leaked.

## Roadmap

These are the areas where we plan to spend our effort and where we think contributions will be most useful.

### Clarity
We're not satisfied with an archive of proof-of-concept apps. The examples in this repository should read like Knuth-style literate programs, making it abundantly clear how the infoleak functions end-to-end: what setup is strictly necessary; what implementation behaviors we're relying on or working around; and what we've added to make the demonstration more robust.

This standard applies to our examples, the library code they share, and the support infrastructure we provide to get things running.

We're always on the lookout for ways we can make examples in this project simpler to understand. We expect that will most often mean better comments and better factoring of code. But sometimes it will mean step-function improvements through entirely new, more straightforward approaches.

### Robustness
These examples are each designed to exercise specific vulnerabilities, and they should produce consistent and useful results anywhere those leaks are present.

This could mean amplifying or cleaning up side-channel signal (on the producer or consumer side) or retrying on failure.

### Breadth
This project hopes to provide examples of every kind of software-observable side-channel. They should run in as many environments as they can to enable comprehensive testing of mitigations. We think we're off to a good start, but we hope to improve our coverage along several dimensions.

#### Leak vectors
We should have examples embodying most or all known timing leak sources, including (non-exhaustively): Spectre (all variants), Meltdown, Speculative Store Bypass, L1 Terminal Fault (L1TF), and Microarchitectural Data Sampling (MDS).

#### Compilers
Our examples should compile with GCC, Clang, and Microsoft Visual C++ (MSVC). (Stretch: ICC.)

#### Operating systems
Our examples should build and produce useful results on macOS, Windows, and Linux -- including, where possible, when those OSes are running virtualized. Eventually we also hope to provide something worth running on iOS and Android.

We intend to explore more virtualized scenarios over time, in which case we will want to cover a diversity of hypervisors and VMMs.

#### Leak gadgets
Our examples should cover known ways of establishing covert side-channels. Data or instruction cache timing are most popular, but we should also explore execution unit contention ([SMoTherSpectre](https://arxiv.org/abs/1903.01843)) or activation latency ([NetSpectre](https://arxiv.org/abs/1807.10535)).

#### Security domains
We want to show information leaks across a variety of typical security boundaries. In what we think is roughly increasing order of difficulty:
- Same user address space (e.g. code loaded in a sandbox)
- User to kernel
- Two user processes on the same kernel
- VM guest to VM host kernel
- Two VM guests on the same host
- Host to BIOS/SMM
- Host to trusted execution environment (TEE) / enclave
- Two systems connected to a fast local network

We expect many of these examples will require turning off existing mitigations or security features that are widely deployed. When we want to demonstrate an attack that isn't possible on a modern, patched system, we'll provide infrastructure to create an old, unpatched system.

#### Platforms
We want examples that provide clear positive (or negative) results across a wide range of processor generations from different vendors across different architectures. That said, we'll also generally prioritize working examples for hardware with the broadest deployment.

#### Target code
We want to demonstrate timing leaks against analogues of different kinds of data-handling code: serialization, cryptographic algorithms, application business logic, etc.

### Quantitative evaluation
We believe the robustness of our samples, and the usefulness of potential mitigations, can be evaluated by extracting metrics that can be compared across instances. Bandwidth -- correct bytes leaked per unit time -- seems like one obvious choice.

### Infrastructure
We want to make it easy to run many examples across many environments quickly and reproducibly. This supports our mission of enabling quantitative comparison, and should allow for a quick feedback loop for developers prototyping new software-based mitigations.

One of the goals of producing a broad set of examples is to be able to show the effectiveness of mitigations, so we also want to add infrastructure to build and test with existing mitigations enabled.

For example:
- MSVC: `/Qspectre`
- LLVM: `-mspeculative-load-hardening`
- GCC: various flags like `-mfunction-return` and `-mindirect-branch`
- ICC: [similar to GCC](https://software.intel.com/en-us/articles/using-intel-compilers-to-mitigate-speculative-execution-side-channel-issues)

## Contributing

We eagerly welcome contributions. Please take a look at our [contributing guidelines](CONTRIBUTING.md) for more on how to engage with the project.

### License

Safeside is dual-licensed under both the [3-Clause BSD License](LICENSE) and [GPLv2](LICENSE.GPL-2.0). You may choose either license.

## Questions?

If you've found a bug or think something's missing, please [file an issue](https://github.com/google/safeside/issues).

For general discussion about side-channels or for questions about the project's goals and roadmap, use our [`safeside-discuss` group](https://groups.google.com/forum/#!forum/safeside-discuss).

## Disclaimer

This is not an officially supported Google product.
