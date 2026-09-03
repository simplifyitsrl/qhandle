# 04 — Product, MVP scope, demo, technical risks

## Main user flow

**Registering** — two transactions, by design:

```
1. Client computes  commitment = K12(canonical_name, owner, random_salt)
2. tx -> CommitRegistration(commitment)        # reveals nothing; free
   ... wait >= 5 ticks (~10s) ...
3. tx -> RegisterHandle(name, target, salt, years) + fee
```

Not incidental: a single-transaction register broadcasts the desired name in the clear before it is
mined, so anyone watching can outrun it — ENS adopted commit–reveal for exactly this reason. Cost is
one extra transaction against Qubic's 1024 tx/tick ceiling, judged worth paying because a registry
that can be sniped has no credible claim to the names it sells.

**Resolving** — free, read-only, never metered:

```
ResolveHandle("alice")  -> { target, owner, expiryEpoch, status }
ReverseResolve(address) -> { name, expiryEpoch, status }
```

An expired handle resolves to `NULL_ID` with `status = EXPIRED`, never to its stale target — a
wallet must not be able to pay the previous holder of a lapsed name. Being `PUBLIC_FUNCTION`s, both
keep serving even if the contract's fee reserve is depleted.

**Lifecycle:** `RenewHandle` (permissionless — anyone may renew anyone's handle, removing the
"owner went dark and the name died" failure mode) · `TransferHandle` · `SetTarget` (repoint without
transferring, so a cold key can own the name while it resolves to a hot address) · `SetPrimary` ·
`SetLock` · `ReclaimExpired` (permissionless sweep).

## MVP scope

**In:** the registry contract; commit–reveal registration; forward and reverse resolution; renewal,
transfer, retarget, lock; expiry with grace period; permissionless reclamation; fee accounting
splitting revenue between dividends and the execution fee reserve.

**Deliberately out of v1**, to keep audit surface and permanent namespace decisions small:
subdomains · auctions or a secondary marketplace · records beyond a single target address (each is
permanent state cost across all 104,857 slots) · handles under 3 characters (reserved — releasing
them can never be undone) · Unicode (a security decision, not a scope cut — R3 in
[10](10-risks-security.md)).

## Reproducible walkthrough

From a clean checkout, with `git clone https://github.com/qubic/core` alongside:

```bash
# 1. Name-validation layer: 28 assertions
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src \
    -o /tmp/tc tests/test_canonicalize.cpp && /tmp/tc

# 2. Stateful contract behaviour: 53 assertions
g++ -std=c++20 -march=native -w -I tests -I core/src -I core -I src \
    -o /tmp/tqs tests/test_qhandle_state.cpp && /tmp/tqs

# 3. State layout is what we claim
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src \
    -o /tmp/ms tests/measure_sizes.cpp && /tmp/ms
```

Captured output is in [`../data/`](../data/), but the point is that it re-runs.
`tests/generate_testnet_payloads.sh` additionally generates and size-checks the `qubic-cli`
payloads for all 11 entry points — it contacts no node, and says so.

## Technical risks

Enumerated in [10](10-risks-security.md). The one shaping the plan is **R1**: static memory makes
launch capacity potentially permanent, which is why `MIGRATE` prototyping is milestone 1 rather
than milestone 4.

## Not yet tested

Nothing has run against a live node — testnet execution is M2. The `qpi.burn()` failure path is
uncovered because the harness always reports success. The contract has not been through the
official [contract-verify](https://github.com/qubic/contract-verify) tool.
