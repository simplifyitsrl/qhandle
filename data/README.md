# Evidence pack

Captured output of claims made in the proposal. Everything here re-runs from a clean checkout —
see [`../docs/04-product-demo.md`](../docs/04-product-demo.md#reproducible-walkthrough).

| File | Claim it evidences | Regenerate with |
|---|---|---|
| `state-size-measurements.txt` | Contract state is 22.34 MB / 104,857 usable handles; `CommitmentPreimage` has no padding | `tests/measure_sizes.cpp` |
| `test-canonicalize-output.txt` | Name validation passes 28 assertions incl. two aliasing attacks | `tests/test_canonicalize.cpp` |
| `test-qhandle-state-output.txt` | Stateful contract behaviour passes 53 assertions (commit-reveal, expiry/grace, fees, END_TICK burn/dividends) | `tests/test_qhandle_state.cpp` |

All of this runs against contract code linked into a test harness. **Nothing here has run against
a live node** — testnet execution is milestone 2. `tests/generate_testnet_payloads.sh` generates
and size-checks the qubic-cli payloads but contacts no node.

Measurements produced 2026-09-03 with g++ (C++20, `-mavx2`) against `qubic/core` at the commit
recorded in the repo history. Values are compiler-computed, not hand arithmetic.
