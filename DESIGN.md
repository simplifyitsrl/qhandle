# QHandle — Registry Data Structure Design (v0.3)

Status: **Implemented and self-reviewed. Not audited.** `src/QHandle.h` follows this design, with
front-running-resistant commit–reveal, soft capacity caps enforced (80% load factor), unrolled
stack-less `Name::operator==`, and capacity fixed at 2^17 (104,857 usable handles).

No external audit has taken place. The Incubation Program funds one audit per contract, and that
is scheduled for milestone 4 — after the stateful suite, `contract-verify`, the `MIGRATE`
prototype, and testnet validation are all complete.

Measured against the built contract (`tests/` harness), the sizing below is verified:
`sizeof(Record)` is 80 bytes as predicted and the whole state is **22.34 MB**
(2.2% of the 1 GB limit), giving **104,857** usable handles at 80% soft capacity limit.
A test suite (`test_canonicalize.cpp`, 28 assertions; `test_qhandle_state.cpp`, 53 assertions)
exercises the contract against real QPI headers, and `tests/` carries `qubic-cli` payload
generators. **No test yet runs against a live node** — see §9.

Every decision below cites either a fetched doc in `reference/docs/` or real code in
`core/src/`. Anything I could not ground is in [§8 Open / flagged](#8-open--flagged).

---

## 1. Three corrections to the brief

These change the design materially, so they come first.

### 1.1 `HashMap` exists in QPI and is allowed in state

`qhandle.json` says the data structure should be a *"fixed-size preallocated array of registration
slots with internal indexing/hashing logic"* — i.e. hand-roll a hash table. **That is not
necessary.** `core/src/qpi/qpi_containers.h:312` defines `QPI::HashMap<KeyT, ValueT, L>` and
`:410` defines `HashSet`, both open-addressed with linear probing and 2-bit occupation flags.

The brief's `allowed_types` list ("uint64, sint8, bit, id, Array, BitArray") is the restriction on
the **public interface only** — `core/doc/contracts.md:632` scopes it that way, and it names only
`Collection` and `LinkedList` as forbidden types. `HashMap` in `StateData` is standard production
practice: `Escrow.h:160`, `GGWP.h:133`, `Qusino.h:239`, `QDuel.h:73`.

**Consequence:** hand-rolled probing logic is deleted from the plan. It would be strictly more
audit surface for the same behaviour, and the incubation program only covers one audit
(`reference/docs/incubation_program.md`, phase 5).

### 1.2 The fee model is *execution fee reserve*, not a per-byte storage tax

`qhandle.json.technical_constraints.storage_fees` describes a per-tick tax on occupied space. The
actual mechanism (`core/doc/execution_fees.md`) is:

- Each contract has an `executionFeeReserve` in Contract 0, seeded at IPO to
  `finalPrice * 676`. At zero the contract goes **dormant** — procedures stop, `BEGIN_TICK` /
  `END_TICK` stop. Read-only `PUBLIC_FUNCTION`s keep working.
- It is drained by measured execution time × a computor-voted multiplier
  (`SPECIAL_COMMAND_SET_EXECUTION_FEE_MULTIPLIER`, `core/src/qubic.cpp:2345`).
- **Plus, on every tick where the state was touched via `state.mut()`, the full state digest is
  recomputed and charged** (`core/doc/contracts.md:33,44-45`). The doc warns this "may be
  significantly more expensive than the run-time of the procedures."
- It is refilled only by `qpi.burn()`.

The Qx "storage fee" bullet in `reference/docs/qx_fee_model.md` is about Qx's *asset balance*
accounting, not a generic network charge. Reusing it as QHandle's cost model would be wrong.

**Consequence — this is the single most important constraint on the design:** cost is driven by
**total state size × number of state-changing ticks**, not by number of handles. A big
preallocated state is charged on every registration tick even if it is 1% full. This is what
answers `open_questions[0]`, and it points the opposite way from "preallocate generously."

### 1.3 `Array` cannot be a `HashMap` key

`QPI::Array` defines no `operator==` (only `BitArray` does, `qpi_containers.h:67`), and
`HashMap::getElementIndex` requires `_elements[index].key == key`
(`core/src/qpi/impl/qpi_hash_map_impl.h:79`). So the handle key must be a struct that supplies
`operator==` itself — the `Qusino.h:225` `VoteInfo` pattern.

---

## 2. Why the key is a wrapper struct and not an `id`

A handle is ≤32 bytes, and `id` is exactly 32 bytes with a working `operator==`. Tempting. **Reject
it**, for one reason:

```cpp
// core/src/qpi/impl/qpi_hash_map_impl.h:27
template <> inline uint64 HashFunction<m256i>::hash(const m256i& key) { return key.u64._0; }
```

For `id` keys the hash is **the first 8 bytes only**. If handles are stored as raw characters, that
means the hash is the **first 8 characters**. Handle namespaces cluster hard on prefixes
(`crypto…`, `qubic…`, `official…`). Worse, it is a cheap griefing vector: register a few thousand
handles sharing an 8-char prefix, and every lookup and insert in that region degrades to a linear
scan — paid for by the contract's execution fee reserve, not the attacker.

The generic template hashes the whole key with KangarooTwelve
(`qpi_hash_map_impl.h:18-23`), so any key type that is *not* `m256i` gets full-width hashing for
free. Hence:

```cpp
struct QHANDLE_Name          // sizeof == 32, alignof == 1, no padding
{
    Array<uint8, 32> chars;  // canonical: zero-padded to 32, lowercase, no NULs inside

    bool operator==(const QHANDLE_Name& other) const
    {
        // no '[' ']' allowed (core/doc/contracts.md:601); Array::get is the accessor
        for (uint64 i = 0; i < 32; ++i)
            if (chars.get(i) != other.chars.get(i)) return false;
        return true;
    }
};
```

**Canonicalisation is a correctness requirement, not hygiene.** The generic hash runs over
`sizeof(KeyT)` raw bytes, so any non-zero garbage in the tail produces a different hash for the
same name. Every entry point must normalise before touching the map. That is what
`_Canonicalize` in §5 is for, and it is the first thing an auditor should be pointed at.

---

## 3. Record layout

```cpp
struct QHANDLE_Record        // 80 bytes (76 used + 4 tail pad); alignof 8
{
    id     owner;            // 32 — may transfer/renew/repoint. Not necessarily the target.
    id     target;           // 32 — what ResolveHandle returns.
    uint32 expiryEpoch;      //  4 — exclusive: expired once epoch >= expiryEpoch
    uint32 registeredEpoch;  //  4
    uint8  length;           //  1 — 3..32, real length; lets clients render exactly
    uint8  flags;            //  1 — bit0 = locked (owner disabled transfer)
    uint16 reserved;         //  2 — explicit, so layout is not compiler-dependent
};
```

`m256i` has no `__m256i` member (`core/src/platform/m256.h:9-40`), so `alignof(id) == 8`, not 32 —
the layout above is stable and I am not relying on a lucky packing.

`owner` and `target` are separate on purpose: a cold-storage key can own `alice` while it resolves
to a hot spending address, and repointing does not require moving ownership. ENS makes the same
split (registrant vs. resolver record).

**Expiry is stored in epochs, not ticks.** `qpi.epoch()` is a `uint16`
(`core/src/qpi/qpi_context.h:78`); an epoch is ~1 week, so a 1-year registration is `+52`. Ticks
would need per-tick comparison against a moving target for no added precision. Stored widened to
`uint32` so `expiryEpoch = epoch + 52*years` cannot wrap during arithmetic.

---

## 4. State

```cpp
struct StateData
{
    HashMap<QHANDLE_Name, QHANDLE_Record, QHANDLE_CAPACITY>   _handles;   // forward
    HashMap<id,           QHANDLE_Name,   QHANDLE_CAPACITY>   _primary;   // reverse

    uint64 _registrationFeePerYear;
    uint64 _renewalFeePerYear;
    uint64 _transferFee;
    uint64 _earnedAmount;
    uint64 _distributedAmount;
    uint64 _burnedAmount;
    uint64 _burnRatePercent;   // share of revenue burned to refill executionFeeReserve
};
```

`_primary` is the reverse record (address → the one handle a wallet should display). It is
**opt-in and explicitly set by the owner**, exactly like ENS reverse resolution — deriving it
automatically from `target` would let anyone squat a display name on someone else's address by
pointing a handle at it.

### Capacity — the actual decision

`L` must be `2^N` (static_assert, `qpi_containers.h:118`). Per-slot cost:

| Map | Element | + occupation flags | Bytes/slot |
|---|---|---|---|
| `_handles` | 32 key + 80 value | 0.25 | 112.25 |
| `_primary` | 32 key + 32 value | 0.25 |  64.25 |
| | | | **176.5** |

`HashMap` access is ~constant below 80% population and degrades toward linear above 90%
(`qpi_containers.h:309-310`), so plan on **0.8·L usable**.

| `L` | State | Usable handles | Verdict |
|---|---|---|---|
| 2^17 = 131,072 | ~23.1 MB | ~104,000 | **Recommended for launch** |
| 2^18 = 262,144 | ~46.3 MB | ~209,000 | Next EXPAND step |
| 2^19 = 524,288 | ~92.5 MB | ~419,000 | |
| 2^20 = 1,048,576 | ~185.1 MB | ~838,000 | Rejected at launch |

**Recommendation: launch at 2^17 (~23 MB).** Justification is §1.2: the digest of the *entire*
state is rehashed on every tick that registers, renews or transfers a handle. At 2^20 the contract
pays to rehash 185 MB per active tick from day one, against a realistic first-year population in
the low thousands. `core/doc/execution_fees.md` states the recommendation directly: "Select a
reasonable size for arrays and other containers, because costs for digest computation increase
with state size." Growing later is a supported, *planned* operation — `PADDING`
(`core/doc/contracts.md:650`) zero-extends the old state file if new structures are appended at the
end, and `MIGRATE` handles the rest.

**But note the trap this creates**, because it is not obvious and it is the thing most likely to
bite us: `PADDING` only works for structures **appended at the end**. Growing `_handles` from 2^17
to 2^18 in place is not appended-at-the-end — it changes the offset of everything after it, and a
`HashMap`'s contents are position-dependent (index = `hash & (L-1)`), so a zero-padded old file is
garbage at the new `L`. Capacity growth therefore requires a **`MIGRATE` procedure that re-inserts
every element into the resized map**, with an `OldStateData` struct and `QHANDLE_CAPACITY_OLD`
kept around. This must be designed in *now*, not retrofitted. It is the strongest argument
against starting large — but also the reason the migration path has to be proven on testnet before
mainnet, and I would make it milestone 3's main exit criterion.

---

## 5. Interface sketch

Public input/output structs may use only integer/bool types, `id`, `Array`, `BitArray`
(`core/doc/contracts.md:631`). `QHANDLE_Name` contains only an `Array<uint8,32>`, so it is legal
across the boundary. `HashMap` never crosses it.

| # | Kind | Name | Notes |
|---|---|---|---|
| F1 | `PUBLIC_FUNCTION` | `Fees` | mirrors `Qx.h:272` |
| F2 | `PUBLIC_FUNCTION_WITH_LOCALS` | `ResolveHandle` | name → `target`, `owner`, `expiryEpoch`, `found`, `expired`. Free; read-only; works even if the reserve is dormant (`execution_fees.md`). |
| F3 | `PUBLIC_FUNCTION_WITH_LOCALS` | `ReverseResolve` | address → primary handle |
| F4 | `PUBLIC_FUNCTION` | `Stats` | population, capacity, reserve level via `qpi.queryFeeReserve` |
| P1 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `RegisterHandle` | name, target, years |
| P2 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `RenewHandle` | name, years — **permissionless**, anyone may renew anyone's handle (ENS does this; it removes the "I lost my key and the name dies" failure mode and cannot harm the owner) |
| P3 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `TransferHandle` | name, newOwner — owner only |
| P4 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `SetTarget` | name, newTarget — owner only, repoint without transferring |
| P5 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `SetPrimary` | opt-in reverse record |
| P6 | `PUBLIC_PROCEDURE_WITH_LOCALS` | `ReclaimExpired` | see below |
| p7 | `PRIVATE_FUNCTION_WITH_LOCALS` | `_Canonicalize` | validate + normalise; the security-critical one |

Every procedure must validate **before** the first `state.mut()` — `execution_fees.md` best
practice 1 — because `state.mut()` is what marks the state dirty and triggers the full-state
digest recompute. A rejected transaction that has already dirtied the state has cost the reserve
the entire rehash for nothing, which is a cheap drain attack.

### `ReclaimExpired` — the part static memory forces on us

Expired records do not free themselves. With a fixed `L`, unreclaimed expiries are a slow leak
that ends in a full map. `HashMap` gives `removeByKey()` (mark) and `cleanup()` (compact, expensive,
invalidates indices) plus `needsCleanup(pct)` — `Qx.h:1152` runs exactly this pattern in `END_TICK`
at a 30% threshold.

Proposal: `ReclaimExpired(name)` is permissionless and free, deleting any record with
`epoch >= expiryEpoch` (plus a grace period). Compaction runs in `END_TICK` guarded by
`needsCleanup(30)`, matching Qx. Registration over an expired-but-not-yet-reclaimed name must also
reclaim inline, so a squatted-then-expired name is never unobtainable just because nobody swept it.

---

## 6. Handle format

- **Length 3–32.** Upper bound is the 32-byte key; sub-3 reserved for a later premium tier (once
  released they can never be un-released).
- **Charset: `a`–`z`, `0`–`9`, `-`.** No leading/trailing/double hyphen.
- **ASCII only — Unicode rejected.** This answers `open_questions[2]` and I want to be firm on it:
  correct Unicode handling needs NFC normalisation and confusable-script detection, and
  `core/doc/contracts.md:23` allows **no external libraries at all**. Without normalisation, two
  byte-different strings that render identically both register — which in a *payment address
  resolver* is a phishing primitive, not a cosmetic issue. Emoji handles are not worth shipping an
  address-substitution attack.
- Note `'` and `"` are forbidden in contract code (`core/doc/contracts.md:612`), so charset checks
  are numeric: `c >= 97 && c <= 122` for `a`–`z`, `c >= 48 && c <= 57` for digits, `c == 45` for
  `-`. Uppercase input (`c >= 65 && c <= 90`) is lowercased by `+32` rather than rejected, so
  `Alice` and `alice` cannot both exist.

---

## 7. Fees

Revenue is dividends to the 676 IPO shareholders via `qpi.distributeDividends` (`Qx.h:1152`) —
**minus a burn slice**. `qpi.burn()` is the only way to refill `executionFeeReserve`, so a QHandle
that distributes 100% of revenue eventually goes dormant and stops resolving. `_burnRatePercent`
is therefore in state from day one and should be shareholder-adjustable, per
`execution_fees.md` best practice 4.

Structure: per-year pricing, `years` bounded 1–5, with length-tiered registration (3-char > 4-char
> 5+) as the standard anti-squat lever. Renewal is flat by length.

I am **not proposing numbers yet.** The floor is set by the execution fee multiplier, which is
computor-voted and not published as a constant — see §8.

---

## 8. Open / flagged

Ranked by how much they block progress.

1. **Execution fee cost per tick is unmeasurable from the docs.** The multiplier is set by quorum
   at runtime; no rate constant is published. Fees cannot be calibrated by calculation, only by
   measuring a deployed contract of known state size on testnet with
   `qubic-cli -qutilqueryfeereserve`. **This should become an explicit milestone-3 deliverable**;
   it currently is not one in `qhandle.json.milestones`. Everything in §7 is blocked on it.
2. **Mempool front-running is closed; out-of-band front-running is not.** `_commitments` keys
   entries by `K12(CommitmentKeyPreimage { qpi.invocator(), input.commitment })`. An adversary
   observing a commitment hash in the unconfirmed transaction pool can neither steal nor
   overwrite the committer's slot. This fixed a real DoS in the earlier design: an observer could
   claim the slot first, after which the victim's own commit hit the idempotent branch and their
   reveal failed the committer check — permanently blocking that name. Someone who learns your
   intended handle *out of band* can still race you; that is inherent to first-come-first-served
   naming and is not claimed to be solved.
3. **`MIGRATE`-based capacity growth (§4) must be prototyped on testnet before mainnet.** If
   re-inserting ~100k elements does not fit in one `MIGRATE` invocation, the whole
   start-small-and-grow strategy fails and capacity must be chosen once, permanently, at launch.
   This is the largest single technical risk in the project.
4. **The `/build` application form fields could not be fetched** — client-side rendered. See the
   "Gap" note in `reference/docs/build_on_qubic.md`. Needs a human with a browser before the
   incubation application is drafted; I did not guess at the proposal structure.
5. **Grace period length after expiry** (ENS uses 90 days ≈ 13 epochs) — implemented as 13 epochs.
6. Not yet designed: whether QHandle should own asset management rights at all
   (`PRE_ACQUIRE_SHARES` / `PRE_RELEASE_SHARES`, `Qx.h:1178`). Current answer is no — QHandle
   issues no assets — explicit reject stubs prevent any unauthorized share movements.

---

## 9. Implementation notes

`src/QHandle.h` implements the above. Verified:

- Compiles clean against real QPI headers (`g++ -std=c++20 -fsyntax-only`, core's own contract prologue).
- Passes automated static scanning with **0 violations** for every construct forbidden by
  `core/doc/contracts.md`: no raw pointers (`*` only for scalar multiplication), no `[` `]`, no `#`,
  no string or char literals, no `/` or `%` (uses `div()`), no `union`, `typedef`, or `__`.
- Unrolled 32-way byte comparison in `Name::operator==` eliminates stack variable allocation inside
  hash map lookups and equality checks.
- Enforces an explicit 80% soft capacity limit (`QHANDLE_MAX_USABLE_CAPACITY = 104857`,
  `QHANDLE_COMMIT_MAX_USABLE_CAPACITY = 3276`) to prevent linear probing degradation.
- **Unit test suite (`tests/test_canonicalize.cpp`)**: 28 assertions on `_Canonicalize`, all passing,
  covering case folding, charset, hyphen placement, length bounds, non-ASCII rejection, and the
  two aliasing attacks (embedded NUL, dirty padding).
- **Stateful integration test suite (`tests/test_qhandle_state.cpp`)**: 53 assertions, all passing,
  covering:
  1. Fees and capacity stats reporting (usable soft cap).
  2. Front-running defense and committer slot isolation.
  3. Registration fee deductions, multi-tier pricing (3, 4, 5+ characters), and excess fee refunds.
  4. Forward resolution (active vs. expired returning `ERR_EXPIRED` and `NULL_ID`).
  5. Primary reverse resolution (`SetPrimary`, `ReverseResolve`).
  6. Target repointing (`SetTarget`).
  7. Transfer lock prevention (`SetLock`, `TransferHandle`) and primary record cleanup on transfer.
  8. Permissionless renewal (`RenewHandle`) and grace period arithmetic.
  9. Expired handle sweeps (`ReclaimExpired`).
  10. Stale commitment garbage collection (`PruneCommitment`).
  11. `END_TICK` 30% execution fee reserve burn and 70% shareholder dividend distribution.
- **`qubic-cli` tooling** (payload generation only — nothing executes against a node yet):
  - `tests/qhandle_cli_helper.cpp`: C++ tool for K12 commitment generation and exact struct
    encoding. Cross-checked: it produces byte-identical commitments to the contract's own
    `ComputeCommitment` for the same inputs, which is the property the whole commit–reveal flow
    depends on.
  - `tests/qubic_cli_payloads.py`: Python module producing payloads identical to the C++ helper.
  - `tests/generate_testnet_payloads.sh`: derives two identities and generates + size-checks all
    11 payloads against the contract's input struct sizes, then prints the commands to run.
    It contacts no node and verifies no on-chain behaviour; it exits non-zero if any payload
    fails to generate or is the wrong size.
- Struct layouts measured, not assumed:
  - `sizeof(Record)` = 80 bytes
  - `sizeof(Name)` = 32 bytes
  - `sizeof(Commitment)` = 40 bytes
  - `sizeof(CommitmentPreimage)` = 72 bytes
  - `sizeof(StateData)` = 23,430,216 bytes (~22.34 MB, ~2.2% of 1 GB limit)

All bugs found during **self-review** — burn-failure accounting, `SetPrimary` overwrite at
capacity, `NULL_ID` validation, the commitment front-running DoS, and linear-probe degradation at
high load — have been fixed, and all but the first are covered by tests.

### Known gaps in verification

Stated explicitly, because the claims above are only worth what they exclude:

- **The `qpi.burn()` failure path is untested.** The test harness's `burn()` always reports
  success, so the `burnResult >= 0` guard never exercises its false branch — and that guard is
  the fix for the subtlest of the bugs above.
- **Nothing has run against a live node.** All 81 assertions execute against contract code linked
  into a test harness, not a deployed contract. Testnet execution is milestone 2.
- **Client-side tooling does not validate handle rules.** The C++ and Python encoders will happily
  encode `ab`, `-alice`, or `al--ice` into payloads the contract rejects; they do not mirror
  `_Canonicalize`.
- **`qubic_cli_payloads.py` prints identities with a placeholder checksum** (`AAAA`) rather than
  the real K12 checksum, so identities it displays are not valid Qubic identities. The C++ helper
  computes the checksum correctly.
- The contract has not been run through the official
  [Qubic Contract Verification Tool](https://github.com/qubic/contract-verify); the local scan
  approximates it but is not a substitute.


---

## 10. Security surface (detail)

Summarised in [`docs/10-risks-security.md`](docs/10-risks-security.md); the full table lives here
so the proposal stays within its page budget.

### Namespace integrity

Name canonicalisation is the most security-critical function in the contract — every name passes
through it — which is why it has dedicated coverage ahead of everything else
(`tests/test_canonicalize.cpp`, 28 assertions).

| Attack | Mitigation |
|---|---|
| Homograph names (`аlice` with Cyrillic а) resolving to an attacker | All non-ASCII rejected. NFC normalisation needs libraries the platform forbids, so the charset is restricted to `a-z`, `0-9`, `-`. Tested |
| Case aliasing — `Alice` and `alice` as two records | Folded to lowercase; one key. Tested |
| Padding / embedded-NUL aliasing — two byte patterns rendering identically | Canonical form zero-fills the tail; embedded NUL followed by non-zero is rejected. Explicit test cases |
| Hyphen tricks (`-alice`, `al--ice`) | Rejected: no leading, trailing or doubled hyphen. Tested |

### Registration integrity

| Attack | Mitigation |
|---|---|
| Mempool sniping of a pending registration | Commit–reveal: `K12(name, owner, salt)` reveals nothing |
| Replaying an observed commitment | Storage key is `K12(committer, commitment)`, so slots are per-committer |
| Dictionary attack over likely names | Salt makes precomputation infeasible |
| Commitment spam or hoarding | Commitments expire and are prunable permissionlessly; `END_TICK` compacts at a 30% threshold |

### Economic and availability

| Attack | Mitigation |
|---|---|
| Draining the fee reserve via failed transactions — the attack `core/doc/execution_fees.md` warns about explicitly | Every procedure validates *fully before the first `state.mut()`*, since that is what triggers the 22 MB digest recompute. The main structural reason the code is written the way it is |
| Reserve depletion over time | A fixed share of revenue is burned into the reserve *before* dividends are distributed |
| Prefix-clustering griefing to degrade lookups | Avoided at the type level: an `id` key hashes only its first 8 bytes (= the first 8 characters); the wrapper-struct key gets the full-width KangarooTwelve hash |
| Squatting / registry exhaustion | Annual renewal with expiry, length-tiered pricing, permissionless `ReclaimExpired` |
| Locking a handle out of existence | `NULL_ID` rejected as owner and as target |

### Platform assumptions

QHandle adds no external dependency — QPI is the only one the platform allows. It assumes what
every Qubic contract assumes: `qpi.invocator()` identifies the caller (all ownership checks rest
on it), `qpi.K12()` is collision-resistant (commit–reveal binding rests on it), and read-only
functions survive fee-reserve depletion. `qpi.tick()` is assumed monotonic but handled
defensively: a tick lower than the commitment's is treated as *expired*, so it fails closed.
