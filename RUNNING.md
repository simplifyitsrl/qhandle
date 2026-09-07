# Running QHandle locally

Everything here runs offline. Nothing needs a Qubic node, a testnet account, or funds.

## Prerequisites

```bash
# from the parent directory of this repo
git clone https://github.com/qubic/core        # QPI headers the contract builds against
git clone https://github.com/qubic/qubic-cli   # only needed for the payload generator
```

`core/` and `qubic-cli/` are gitignored on purpose: they are upstream clones, not our code.

You need `g++` with C++20 and a CPU with AVX2/BMI (any x86-64 from the last decade).

## 1. Does the contract compile?

The contract is a header included into Qubic core's contract build with four macros defined
around it. This reproduces that setup without building the whole node:

```bash
cat > /tmp/syntax.cpp <<'CPP'
#define NO_UEFI 1
#include "contract_core/pre_qpi_def.h"
#include "qpi/qpi.h"
#define QHANDLE_CONTRACT_INDEX 99
#define CONTRACT_INDEX QHANDLE_CONTRACT_INDEX
#define CONTRACT_STATE_TYPE QHANDLE
#define CONTRACT_STATE2_TYPE QHANDLE2
#include "QHandle.h"
int main() { return 0; }
CPP

g++ -std=c++20 -fsyntax-only -mavx2 -w -I core/src -I core -I src /tmp/syntax.cpp && echo CLEAN
```

## 2. Name validation — 28 assertions

Tests `_Canonicalize`, the function every handle passes through before it reaches the registry.

```bash
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src \
    -o /tmp/tc tests/test_canonicalize.cpp && /tmp/tc
# -> 28 checks, 0 failures
```

Covers case folding (`Alice` and `alice` must be one key), charset, hyphen placement, length
bounds, non-ASCII rejection, and two aliasing attacks — an embedded NUL and dirty padding, both of
which would otherwise let two different byte patterns render as the same name.

## 3. Contract behaviour — 53 assertions

Runs the real contract procedures against a stubbed QPI context: a simulated tick, epoch,
invocator, invocation reward, and a heap-allocated contract state.

```bash
g++ -std=c++20 -march=native -w -I tests -I core/src -I core -I src \
    -o /tmp/tqs tests/test_qhandle_state.cpp && /tmp/tqs
# -> Test Summary: 53 assertions, 0 failures
```

**`-I tests` must come first** — `tests/common_buffers.h` is an empty stub that shadows core's
version, which would otherwise drag in UEFI headers. **`-march=native`** is needed because
KangarooTwelve uses BMI/LZCNT instructions.

Covers commit–reveal including a simulated front-running attempt, fee tiers and refunds, forward
and reverse resolution, expiry and grace arithmetic, transfer locks, renewal, reclamation, and the
`END_TICK` burn/dividend split.

## 4. State layout measurement

```bash
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src \
    -o /tmp/ms tests/measure_sizes.cpp && /tmp/ms
# -> sizeof(StateData) = 22.34 MB, 104,857 usable handles, OK
```

Fails if `CommitmentPreimage` ever acquires padding — it is hashed raw, so padding bytes would make
the same commitment hash differently on another compiler and silently break every registration.

## 5. Payload generator (needs qubic-cli built)

```bash
./tests/generate_testnet_payloads.sh
# -> 11 payloads generated, all correctly sized.
#    Not verified: on-chain behaviour. No node was contacted.
```

Derives two identities, generates the `qubic-cli` payload for all 11 entry points, checks each
against the contract's input struct size, and prints the commands you would run. It contacts no
node. Seeds are never printed — generated commands show `<ALICE_SEED>` as a placeholder.

Individual payloads:

```bash
./tests/qhandle_helper compute-commitment alice <IDENTITY> 12345
./tests/qhandle_helper encode-register alice <IDENTITY> 12345 1
./tests/qhandle_helper                      # lists all 15 commands
```

The Python module `tests/qubic_cli_payloads.py` produces byte-identical payloads and can be
imported as a library.

## Run everything

```bash
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src -o /tmp/tc  tests/test_canonicalize.cpp && /tmp/tc
g++ -std=c++20 -march=native -w -I tests -I core/src -I core -I src -o /tmp/tqs tests/test_qhandle_state.cpp && /tmp/tqs
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src -o /tmp/ms  tests/measure_sizes.cpp && /tmp/ms
```

## What is NOT covered

- Nothing has run against a live Qubic node. Testnet deployment is milestone 2.
- The `qpi.burn()` failure path is untested — the harness always reports success.
- The contract has not been through the official
  [contract-verify](https://github.com/qubic/contract-verify) tool.
