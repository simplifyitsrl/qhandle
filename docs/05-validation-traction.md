# 05 — Validation and traction

## Stage, stated plainly

**Pre-build / prototype.** The program's standard for this stage is *"user interviews, clear ICP, a
prototype or reproducible product artifact, and a concrete why-Qubic argument"* — and explicitly
not *"desk research, trend slides, and a giant market estimate."*

| Required | Have it? | Where |
|---|---|---|
| Clear ICP | **Yes** | [02](02-problem-icp.md) — a named buyer, an explicit non-target |
| Prototype / reproducible artifact | **Yes** | Compiling contract, 81 passing assertions, measured state ([04](04-product-demo.md)) |
| Concrete why-Qubic argument | **Yes** | [03](03-why-now-why-qubic.md) — checkable in one grep of core |
| **User interviews** | **No — not yet done** | Below |

## The only signal we actually have

The founder holds an ENS or equivalent name on **every network where he transacts, except
Qubic** — among the most significant of them. QHandle exists because of that gap.

That is **n = 1, and founder self-report.** We label it rather than inflate it: a real unmet need
experienced by someone who is precisely the secondary ICP, and the honest origin of the project. It
is *not* proof anyone else will pay. We have no interviews, no waitlist, no pilot, no wallet
commitments.

## Why the gap is unusually cheap to close

The primary ICP is **four named teams**, not a market segment. They can be contacted directly and
the result is reviewer-verifiable — an email thread, a call, a written yes or no. That is why
wallet validation is **milestone 1 scope, before the contract is finalised**: the answers change
what gets built.

| Question | If the answer is no |
|---|---|
| Would you surface a third-party name resolver in your send flow? | Primary distribution is gone → **stop or re-scope** |
| Do you already have, or plan, an equivalent feature? | Land-grab argument collapses → **stop** |
| What would block integration — latency, trust, UI, review? | Reprioritise M3 |
| Do you want reverse resolution for display? | If not, `_primary` can be dropped — **8.03 MB of the 22.34 MB state** |
| Would you warn users about a lapsing handle? | Determines whether `ResolveHandle`'s output changes shape |
| What handle length do your users need? | Confirms or refutes the 32-character cap |

Two of these can **shrink the contract**. This stage is about removing state cost, not adding
features.

## Metrics, in the required format

The program requires *"a period, a denominator, and a source."*

| Metric | Period | Denominator | Source |
|---|---|---|---|
| Handles registered | per epoch | — (count) | `Stats`, on-chain |
| Registry utilisation | per epoch | 131,072 slots | `Stats` → population / capacity |
| **Renewal rate** | per epoch cohort | handles reaching expiry that epoch | on-chain: renewals ÷ expiries due |
| Resolve calls | per epoch | — (count) | wallet partner telemetry (a dependency, not an assumption) |
| Fee revenue | per epoch | — (QU) | `Stats` → `earnedAmount` delta |
| Execution fee reserve | per epoch | — (QU) | `qpi.queryFeeReserve`, via `Stats` |

Renewal rate is the one that matters. Registrations are a one-time land-grab and will look
flattering regardless; **renewals are the only evidence the names are used.**
