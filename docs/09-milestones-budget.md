# 09 — Milestones, acceptance criteria, budget

Built against the program's rules: one payable tranche per milestone; user-testable deliverables
only ("refactors, research, architecture, and meetings are not payable on their own"); evidence in
this repo; final milestone = launch at ≥25%.

That last rule shaped the plan: `MIGRATE` prototyping and test work are the most important
engineering we do and are **not payable alone** — so M1 is defined by its verifiable *outputs*.

## M1 — De-risk the irreversible decisions · $3,000

Registry capacity and wallet willingness cannot be fixed later.

**Deliverables:** `MIGRATE` capacity-growth prototype (2^17 → 2^18) with a measured verdict on
whether ~105,000 re-insertions fit one invocation · stateful test suite on core's harness ·
official [contract-verify](https://github.com/qubic/contract-verify) run · written outcomes from
the four wallet teams ([questions](05-validation-traction.md)).

**Acceptance:** a reviewer runs the suite from a clean checkout and it passes · a written go/no-go
on capacity is in `docs/` · `contract-verify` output committed and clean · wallet responses in the
repo, **including refusals**.

**Value:** capacity is re-chosen while that is still free; if wallets decline the project stops at
11.5% spend. *Designed to kill the project cheaply.*

## M2 — Testnet, tooling, measured fee calibration · $4,000

**Deliverables:** contract live on testnet · `qhandle.cpp/h` in a `qubic-cli` fork · **measured**
execution-fee cost per tick and per procedure at this state size · the resulting fee table
replacing the placeholder constants.

**Acceptance:** a reviewer registers and resolves a handle on testnet from a documented command
sequence · `data/` holds the raw measurements with period and source · fee constants are derived
from them, with the derivation written down.

**Value:** first point at which anyone can *use* QHandle, and pricing stops being a guess (R2).

## M3 — Resolver library and one live wallet integration · $5,000

**Deliverables:** resolver library wrapping resolution with expiry handling and a documented
not-found path · **one wallet integration live on testnet** · integration guide for the rest.

**Acceptance:** a reviewer sends a testnet transaction to `something` instead of a 60-character
address, in a real wallet UI, and it arrives · the integration is public and attributable.

**Value:** proves the thesis — revenue depends entirely on handles being typeable in wallets people
already use.

## M4 — Audit and remediation · $5,000

**Deliverables:** audit against the scope in [10](10-risks-security.md#audit-plan) · remediation of
every finding with a linked commit · written response to each, including anything deliberately not
fixed and why.

**Acceptance:** report in the repo · every finding has a resolution and commit reference · test
suite passes after remediation.

## M5 — IPO and mainnet launch · $9,000

**Deliverables:** quorum proposal passed · IPO conducted with **all 676 shares sold** · contract
live on mainnet with index and address published · launch AMA · live registrations by non-team
users.

**Acceptance:** operating on mainnet at a published address · non-team registrations reported per
epoch from on-chain `Stats` · the M3 integration live against mainnet.

**Delivery risk:** if the IPO fails to sell all 676 shares the contract is *permanently broken and
cannot be re-IPO'd*. Outreach is a deliverable here, not marketing.

## Tranche summary

| # | Milestone | USD | Share | Ends in |
|---|---|---:|---:|---|
| M1 | De-risk irreversible decisions | 3,000 | 11.5% | Go/no-go on capacity + wallet answers |
| M2 | Testnet + tooling + fee calibration | 4,000 | 15.4% | Reviewer registers a handle on testnet |
| M3 | Resolver + live wallet integration | 5,000 | 19.2% | Reviewer sends to a name in a real wallet |
| M4 | Audit + remediation | 5,000 | 19.2% | Report + remediation commits |
| M5 | IPO + mainnet launch | 9,000 | 34.6% | Live on mainnet, real registrations |
| | **Total requested** | **26,000** | 100% | |

Final milestone 34.6% ≥ 25% ✓

## Use of funds

**$20,000 engineering time.** Two people at 20 hours per week each over roughly five months —
about **870 person-hours**, a blended **$23/hour**. Plus **$3,000 re-audit contingency** in M4
(Qubic funds one audit; a second is ours) and **$3,000 toward IPO share participation** in M5. The
audit itself is Qubic-funded and deliberately does not appear as a cost line.

**Caveat on the IPO line, stated rather than buried.** The weakest item in the request, and we
expect it questioned. For: *all 676 shares must sell or the contract is permanently broken*, so
participation helps the auction clear. Against: the program pays for outcomes, and buying shares is
not an outcome. **If Qubic prefers, strike it — the request becomes $23,000 and we self-fund.** The
auction price is also unknown until it happens, so $3,000 is an allocation, not a computed cost.

**Cashflow reality:** payment is on delivery with no advance, so M1 is entirely self-funded before
any money arrives — another reason it is scoped to stop the project cheaply.

## Sequencing

**Riskiest work first:** if capacity growth is one-way, everything after M1 rests on a permanent
decision made without evidence. **Wallet validation precedes the resolver:** building an integration
nobody agreed to surface is the "long build plan before the first real user" weak signal.
