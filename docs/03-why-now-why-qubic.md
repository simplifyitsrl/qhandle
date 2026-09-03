# 03 — Why now, and why Qubic

The program screens out "Qubic as a cosmetic chain choice rather than an advantage." The argument
here is the opposite: QHandle is *only* interesting on Qubic, and it is hard *because* of Qubic.

## 1. The standard implementation is impossible here

Naming services elsewhere mint one token per name and store the name in free-form metadata — ADA
Handle does this. On Qubic:

```
Asset names consist of up to 7 characters. The first must be an upper-case letter in
range A to Z. The following may be either upper-case letters (A to Z) or digits (0-9).
                                          -- core/doc/contracts.md, "Assets and shares"
```

`alice` cannot be an asset. Neither can any lowercase name, any name over 7 characters, or any name
with a hyphen. **The entire NFT-per-name pattern is unavailable**, so a Qubic naming service must be
a registry contract owning its own storage and indexing — substantially more work than minting a
token per name.

That is our answer to "why is there still no working naming service on Qubic." An earlier attempt
exists and went dormant ([06](06-market-gtm-competition.md)); we do not know why, and would rather
ask than assume.

## 2. The memory model makes the design non-obvious

Contract memory is statically allocated, so capacity is a **permanent, pre-committed decision** —
and it interacts with the fee model in a way that is easy to get backwards:

> "every tick that has a change of contract's state (via `state.mut()`) costs fees due to the need
> to recompute the digest of the state. Depending on the size of the state, the digest computation
> may be significantly more expensive than the run-time of the procedures."
> — `core/doc/execution_fees.md`

Cost scales with **total state size × state-changing ticks**, not with number of handles. An
oversized registry pays to rehash its own emptiness on every registration. That is why QHandle
launches at 22.34 MB / 104,857 handles rather than the ~185 MB a naive design would take
([`../DESIGN.md`](../DESIGN.md) §1.2, §4).

## 3. Qubic's revenue mechanism fits a naming service unusually well

Registration and renewal fees are recurring, denominated in QUBIC, and paid by the party who
benefits. Qubic distributes contract revenue natively to the 676 IPO shareholders. A naming service
is one of the few products whose revenue is both predictable and permanently recurring — renewals
do not churn the way trading volume does.

## Why now

- **The ecosystem now has wallets to integrate with.** Four candidates with a send-to-address flow
  exist today. Without wallet surface area this is a curiosity; with four candidates it is
  infrastructure.
- **Naming is a land-grab that happens once.** Whoever ships first defines the namespace.
- **The contract is now buildable and checkable.** `QPI::HashMap` exists in current core, removing
  the need to hand-roll open addressing — the largest chunk of audit surface a 2023-era version of
  this project would have carried.

## What would make this argument wrong

If a wallet shipped its own naming feature and users found it sufficient, a shared registry is much
less valuable — an explicit M1 question, since we cannot verify none is planned. If Qubic changed
addressing at protocol level this becomes redundant; nothing on the published roadmap suggests it,
but we cannot rule it out.
