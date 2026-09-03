# 00 — Executive summary

**QHandle is a naming service for Qubic: it maps `alice` to a wallet address, on-chain, with paid
registration and annual renewal.**

## The problem

Qubic addresses are 60-character strings with no memorable alternative, so every payment, checkout
and transfer depends on copying one verbatim. Ethereum solved this with ENS in 2017, Cardano with
ADA Handle in 2021. Qubic has nothing.

## Who it is for

**Wallet developers** (HASHWallet, Rubic, Qubihub, Qubic-Connect) are the integration buyer — a
send-to-name field removes their highest-friction step. **Repeat receivers** — merchants, creators,
OTC desks, treasuries — are the paying customer. See [02](02-problem-icp.md).

## Why Qubic, and why this has not been built

Not a chain-agnostic idea with Qubic bolted on. No naming service exists here because of a specific,
checkable constraint:

> `qpi.issueAsset()` caps asset names at **7 uppercase characters**. The one-asset-per-name pattern
> ADA Handle and most NFT-based naming services use cannot represent `alice`.

A naming service on Qubic must be a purpose-built registry contract designed against a static memory
model — real, Qubic-specific work. An earlier attempt exists and went dormant; we address that
directly in [06](06-market-gtm-competition.md) rather than claiming the field is empty.

## What exists before any funding

| Artifact | Verifiable how |
|---|---|
| Registry contract, ~1,500 lines of QPI C++ | Compiles clean against real `qubic/core` headers |
| Test suite | 81 assertions passing (28 name-validation, 53 stateful) |
| State layout | Measured: 22.34 MB, 104,857 usable handles |

Settled decisions: commit–reveal registration (a one-shot register is snipeable at the mempool); a
full-width hash key rather than `id` (Qubic hashes only an `id`'s first 8 bytes — for names, the
first 8 characters, a cheap griefing vector); ASCII-only handles (Unicode without NFC normalisation
is a homograph phishing primitive, and contracts may use no libraries).

## Milestones

Five tranches, each ending in something a reviewer can run or read; the final one is mainnet launch
at 34.6%. Full criteria in [09](09-milestones-budget.md).

1. Capacity-migration prototype + stateful tests + wallet answers — *de-risks what cannot be undone*
2. Testnet deployment + `qubic-cli` integration + measured fee calibration
3. Resolver library + one live wallet integration
4. Audit remediation
5. IPO + mainnet launch + AMA

## The ask and the return

**$26,000** — $20,000 engineering across five milestones, $3,000 re-audit contingency, $3,000
toward IPO share participation (with an honest caveat on that last line in [09](09-milestones-budget.md)).

**Return:** Qubic Incubation receives an allocation of the 676 contract shares, earning from
registration and renewal revenue through Qubic's native dividend distribution — same source of
value, no contract logic of ours in the path, no added audit surface ([08](08-return-to-incubation.md)).

## Honest statement of stage

Pre-build/prototype. Against the program's evidence standard we have the prototype artifact and the
why-Qubic argument; we do **not** have user interviews. That is stated plainly in
[05](05-validation-traction.md), and closing it is milestone 1.
