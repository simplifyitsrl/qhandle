# 10 — Risks, security assumptions, audit plan

## Dependency surface

QHandle adds **no external dependency** — QPI is the only one the platform allows. No libraries, no
oracles, no cross-contract calls; it issues no assets and refuses asset management rights. This
narrowness is deliberate, so the one funded audit is spent on registry logic, not integration risk.

Platform assumptions and the full attack-surface table are in [`../DESIGN.md`](../DESIGN.md) §10.
Two items shaped the code and belong here:

- **Name canonicalisation is the most security-critical function**, since every name passes
  through it. It has dedicated coverage ahead of everything else (28 assertions), including
  homograph, case-aliasing and embedded-NUL attacks.
- **Every procedure validates fully before the first `state.mut()`**, because that call triggers
  the 22 MB state digest recompute charged to the fee reserve. A rejected transaction that has
  already dirtied state has burned that cost for nothing — the drain attack the core docs warn
  about.

## Key risks

| | Risk | Response |
|---|---|---|
| **R1** | **Capacity growth may be one-way.** Static memory fixes capacity at 2^17. Growth cannot use the cheap `PADDING` path — a `HashMap` is position-dependent (`hash & (L-1)`), so it needs a `MIGRATE` re-inserting every record. If ~105,000 re-insertions do not fit one invocation, capacity is permanent | Prototyped in **M1**, before most of the budget commits. The only open question that can invalidate a decision already made |
| **R2** | **Fee levels can only be measured.** The execution fee multiplier is set by computor quorum at runtime and is not published. Under-price → dormant contract; over-price → no adoption | Constants marked placeholders in the source; calibration is a named **M2** deliverable |
| **R3** | **Homographs, if Unicode were allowed.** In an address resolver this is a phishing primitive, not cosmetics | ASCII-only, enforced numerically and tested. A real product limitation: no non-Latin handles, ever, under this design |
| **R4** | Front-running by someone who learns your name out-of-band | Not solved, and not claimed. Inherent to first-come-first-served naming; commit–reveal closes only the mempool path |
| **R5** | Reverse records drifting stale after transfer or expiry | `ReverseResolve` validates against the forward record on every read; stale entries are inert |

**Operational:** computors could raise the fee multiplier post-launch — the burn rate is the
lever, and should be shareholder-adjustable before mainnet. Contract code is immutable, so a logic
bug ships permanently (bugfixes are possible coordinated with an epoch update). Owner key loss is
not recoverable by design, but `RenewHandle` is permissionless, so a name does not die because its
owner went dark.

## Three bugs found in self-review, and fixed

An audit should re-check this class: `END_TICK` recorded a burn as complete even when
`qpi.burn()` failed, cancelling the obligation and leaving the reserve unfilled · `SetPrimary`
rejected an *update* by an existing holder at capacity, though overwriting a key does not grow the
population · nothing rejected `NULL_ID`, so a handle could be sent to the null address —
unrecoverable, and it would hand a resolving wallet a null payee.

## Audit plan

One Qubic-funded audit per contract, re-audits at our cost — hence the sequencing: complete the
stateful suite, run `contract-verify`, resolve R1, and finish testnet validation **before** the
audit. Spending the one funded audit on a contract that has not seen testnet would waste it.

**Scope to request:** name canonicalisation · commit–reveal binding · authorisation on every
mutating procedure · expiry/grace/reclaim arithmetic · fee accounting and the burn/dividend split
· the `MIGRATE` procedure, since a migration bug destroys the registry at once. The return
mechanism adds nothing: share assignment ([08](08-return-to-incubation.md)) uses Qubic's native
dividend distribution and needs no contract change.

## Not solving

No Unicode handles ever · no recovery for lost owner keys · out-of-band front-running · no
trademark dispute process. Names are first-come-first-served, and we do not propose becoming an
arbiter of name ownership.
