# 08 — Return to Incubation

> "We no longer deploy capital on a pure non-repayable basis. Each proposal must define one
> primary return mechanism." Guardrail: *"the return must come from the same source of value that
> makes the product useful."*

## What is possible on Qubic

Three platform facts shape this. **Contract revenue does not flow to the team** — fees accumulate
in the contract and `qpi.distributeDividends()` pays them to holders of the 676 IPO shares.
**The IPO pays the team nothing** — proceeds seed the execution fee reserve ([07](07-business-model-pricing.md)).
**The contract is immutable** — anything requiring it to route value must exist before audit and IPO.

The first fact is the useful one: **the dividend machinery that already exists is exactly a
fee-share mechanism.** It pays out of registration and renewal revenue — the same source of value
that makes the product useful — automatically and on-chain.

## Primary mechanism: share assignment

**Qubic Incubation receives a fixed allocation of the 676 QHandle shares**, earning a proportional
share of all contract revenue through Qubic's native dividend distribution.

| Term | Proposal |
|---|---|
| Which fees count | All of them — registration, renewal and transfer, after the reserve burn |
| Calculation base | `shares / 676` of distributed revenue, per epoch |
| How measured | On-chain, by the protocol. No logic of ours in the path, nothing depending on our bookkeeping |
| Start trigger | First distribution after mainnet launch |
| Reporting | None needed — the payments *are* the report; `Stats` also exposes earned/distributed |
| **Allocation size** | **TODO — requires agreement with Qubic** |
| **Who acquires at IPO** | **TODO — see below** |

**Why this over a contract-level fee split**, which we also considered: zero added audit surface ·
no immutable recipient address that could later be lost or compromised · **it unblocks the
contract**, since a fee split had to be designed and frozen before the IPO — choosing shares means
`src/QHandle.h` can go to audit as it stands · alignment is structural, not contractual.

## The one thing to settle

Shares exist only after the IPO and must be **bought** there — the auction pays the fee reserve,
not us. Either **Qubic bids directly** (cleanest; the bid also helps the auction clear, which
matters because if all 676 do not sell the contract is permanently broken and cannot be
re-IPO'd), or **the team acquires and transfers** post-IPO, in which case the purchase must appear
as a use of funds in [09](09-milestones-budget.md). No preference; needs deciding before the IPO
milestone, not before the audit.

## Honest tension

The program asks each mechanism to specify a **cap or duration**. Share assignment has neither —
it is perpetual and uncapped. Flagging rather than dressing up:

**For accepting it:** the claim is proportional, cannot be inflated, and is capped in practice by
QHandle's actual revenue. If handles go unused, Qubic earns nothing; the return cannot detach from
the value created. **If Qubic prefers a cap:** the alternative is the contract-level fee share with
an explicit cap or end epoch. We will build it — but that decision must land **before** the
contract is frozen for audit, since it cannot be added afterwards.

## Not proposed

**No token** — the program restricts token rights to where one is economically necessary, and it
is not; proposing one would be exactly the "token that exists only to manufacture upside"
guardrail. **No off-chain self-reported revenue share** — strictly worse than either on-chain
option. **No equity** — there is no operating company holding the revenue; the contract holds it.
