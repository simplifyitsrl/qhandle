# 07 — Business model and pricing

## How the team makes money

Stated first, because it is the question a reviewer asks first.

**Our income is our shareholding in the contract. There is no second revenue stream.**

That is a consequence of how Qubic works rather than a choice. A contract's income belongs to its
676 IPO shareholders and is paid out by `qpi.distributeDividends()`; there is no team treasury, and
no way to take a cut ahead of distribution without writing one into the contract — which we
deliberately did not do, because it adds audit surface and bakes an immutable recipient address
into an immutable contract.

**The team intends to bid for approximately 34 of the 676 shares (~5%)** at the IPO, on the same
terms as every other bidder. The IPO pays nobody: proceeds seed the contract's execution fee
reserve (`finalPrice × 676`), so we are buyers there, not recipients. After the 30% burn that keeps
that reserve funded, a 5% stake is about **3.5% of gross fees**.

That is thin, and we would rather say so than dress it up. What it buys is alignment that cannot
drift: we have no side revenue that pays whether or not handles are used. If nobody registers and
renews, we earn nothing — the same position Qubic Incubation is in under the return mechanism we
propose ([08](08-return-to-incubation.md)).

Two consequences worth flagging. **All 676 shares must sell or the contract is permanently broken
and cannot be re-IPO'd** — so our participation is not only retained interest, it helps the auction
clear. And our stake competes with Qubic's allocation for the same 676 shares.

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

Revenue is split before it leaves the contract: 30% burned into the execution fee reserve, the
remainder distributed to shareholders. A contract paying out 100% eventually goes dormant.

## Pricing levels — deliberately not stated yet

Fee constants in `src/QHandle.h` are marked **placeholders in the source**. They cannot be derived
from documentation — the execution fee multiplier is set by computor quorum at runtime.
Under-pricing depletes the reserve; over-pricing kills adoption. The only honest way to set them is
to deploy at this exact state size on testnet and measure: **a named M2 deliverable**, whose output
is the fee table. Inventing numbers now would be precise-looking figures with nothing behind them.
