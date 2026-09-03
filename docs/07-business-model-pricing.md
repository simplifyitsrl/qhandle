# 07 — Business model and pricing

## How the product makes money

| Fee | Trigger | Why this pricing metric fits |
|---|---|---|
| **Registration** | Creating a handle, per year, tiered by length | Charges for the scarce thing: an exclusive claim on a short, memorable name |
| **Renewal** | Keeping a handle active, per year | Charges for the cost the handle actually imposes — permanent state in a fixed-size registry |
| **Transfer** | Changing owner | Small; prices the state write, not a revenue line |

**Resolution is free and always will be.** It is a `PUBLIC_FUNCTION`, so it costs the caller
nothing and cannot be metered. Charging wallets per lookup would kill the only distribution channel
that matters.

## Why renewal, not a one-time fee

The load-bearing decision. **For the registry:** every handle occupies one of 131,072 permanent
slots and contributes to a 22.34 MB state whose digest is recomputed on every state-changing tick.
That cost recurs, so the revenue against it must too — a one-time fee funds a permanent liability.
**For users:** without expiry the namespace is squatted once and dead forever; annual renewal is
what returns unused names to supply via permissionless `ReclaimExpired`.

Length tiering (3-char > 4-char > 5+) prices scarcity directly and makes bulk squatting expensive
rather than profitable.

## Unit economics

The constraint that drove the design: **cost scales with total state size × state-changing ticks,
not with number of handles.** Fixed cost is high, marginal cost per handle near zero — so the
contract must be *small* and priced to cover fixed cost at **low** utilisation, which is why
capacity is 2^17 rather than 2^20 ([03](03-why-now-why-qubic.md)).

Revenue is split before it leaves the contract: a fixed percentage is burned into the execution fee
reserve, the remainder distributed to shareholders. A contract paying out 100% eventually goes
dormant.

## Pricing levels — deliberately not stated yet

Fee constants in `src/QHandle.h` are marked **placeholders in the source**. They cannot be derived
from documentation — the execution fee multiplier is set by computor quorum at runtime.
Under-pricing depletes the reserve; over-pricing kills adoption. The only honest way to set them is
to deploy at this exact state size on testnet and measure: **a named M2 deliverable**, whose output
is the fee table. Inventing numbers now would be precise-looking figures with nothing behind them.

## The team's economics

**A Qubic contract IPO does not pay the project team.** Verified:

> "The IPO proceeds generate the **initial execution fee reserve**: `finalPrice × 676`"
> — `reference/docs/smart_contract_lifecycle.md`, Phase 12

So the team's only ongoing revenue is dividends, which requires *holding shares* bought at the IPO
on the same terms as anyone else. **The team intends to bid for approximately 34 of the 676 shares
(~5%)** — the whole of its economic interest in QHandle. This sits alongside Qubic Incubation's
allocation ([08](08-return-to-incubation.md)); both are bought at the same auction, and **all 676
must sell or the contract is permanently broken**. Team participation therefore serves two
purposes: retained interest, and helping the auction clear.
