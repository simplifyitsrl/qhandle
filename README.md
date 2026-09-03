# QHandle

**A human-readable naming service for Qubic: `alice` instead of a 60-character address.**

QHandle is an on-chain registry smart contract that maps short, human-readable handles to Qubic
wallet addresses, with paid registration, annual renewal, and transferable ownership. It is the
ENS equivalent for Qubic — and, for a structural reason explained below, it cannot be built the
way naming services are built on other chains.

- **Status:** working contract, compiling against Qubic core, with a tested validation layer.
  Pre-testnet. See [Try it](#try-it).
- **Submission:** Qubic Incubation Program. The proposal lives in [`docs/`](docs/), starting
  with the [one-page summary](docs/00-summary.md).

## Why it matters

Qubic addresses are 60-character strings with no memorable alternative. Every payment between
users, every merchant checkout, and every wallet-to-wallet transfer requires copying one
verbatim. Qubic has no working naming service today. One earlier attempt was started and has since
gone dormant — that is covered in [`docs/06`](docs/06-market-gtm-competition.md) rather than
skipped. The gap persists not because nobody wants one, but because the obvious implementation is
impossible on this network:

> `qpi.issueAsset()` caps asset names at **7 uppercase characters**. The one-NFT-per-name
> pattern used by ADA Handle and others simply cannot represent `alice` as an asset. A naming
> service on Qubic has to be a purpose-built registry contract, and that is why the gap exists.

Full argument: [`docs/03-why-now-why-qubic.md`](docs/03-why-now-why-qubic.md).

## What exists today

| Artifact | What it is |
|---|---|
| [`src/QHandle.h`](src/QHandle.h) | The registry contract, ~1,500 lines of QPI-restricted C++. Compiles clean against real Qubic core headers. |
| [`tests/test_canonicalize.cpp`](tests/test_canonicalize.cpp) | 28 passing assertions on the name-validation layer, including two aliasing attacks. |
| [`DESIGN.md`](DESIGN.md) | Full design rationale, with measured state sizes and every decision cited to core source or docs. |
| [`reference/docs/`](reference/docs/) | Offline copies of the Qubic documentation the design is grounded in. |

Measured, not estimated: total contract state is **22.34 MB** (2.2% of the 1 GB limit), giving
**104,857** usable handles at the 80% load factor where `QPI::HashMap` stays constant-time.

## How it works

Registration is **commit–reveal**, in two transactions:

1. `CommitRegistration(commitment)` — publishes only `K12(name, owner, salt)`. Reveals nothing.
2. `RegisterHandle(name, target, salt, years)` — reveals the preimage after a short delay.

Without this, a one-shot register broadcasts the desired name in the clear before it is mined,
and can be sniped by anyone watching. A naming service that can be front-run has no credible
claim to the names it sells.

Reads are free and never charged: `ResolveHandle(name) → address` and
`ReverseResolve(address) → name` are `PUBLIC_FUNCTION`s, so they keep working even if the
contract's execution fee reserve is depleted.

## Try it

The contract is not yet deployed. What a reviewer can verify today, from a clean checkout:

```bash
git clone https://github.com/qubic/core          # the QPI headers the contract builds against
g++ -std=c++20 -mavx2 -w -I core/src -I core -I src \
    -o test_canonicalize tests/test_canonicalize.cpp && ./test_canonicalize
# -> 28 checks, 0 failures
```

To confirm the contract itself compiles under the real contract prologue, see
[`docs/04-product-demo.md`](docs/04-product-demo.md#reproducible-walkthrough).

## Who is building it

**[Simplify it S.R.L.](https://www.simplifyit.com.bo)** (Bolivia) — custom Odoo development and
migrations. Two people on QHandle, 20 hours per week each:

- **Grover Hernando Menacho Quisbert** — founder and lead developer. Owns the contract and
  technical decisions. [LinkedIn](https://www.linkedin.com/in/groustuff/)
- **Silvia Mariela Vargas Cordova** — QA and business analyst. Owns test coverage and milestone
  acceptance criteria. [LinkedIn](https://www.linkedin.com/in/silvia-vargas-4213a8311/)

Sixteen years of enterprise software delivery, thirteen in ERP — but this is our first blockchain
product, and [`docs/01-team.md`](docs/01-team.md) says so plainly, along with what we think does
transfer and where we are weak.

## Repository layout

```
README.md                  this file
LICENSE                    MIT
DESIGN.md                  technical design, sizing math, decision log
docs/                      the Incubation Program proposal (00-summary first)
src/QHandle.h              the registry contract
tests/                     verification harness
demo/, data/               evidence pack (measurements, test output, walkthroughs)
```

## License

MIT, © Simplify it S.R.L. — see [`LICENSE`](LICENSE). Permissive on purpose: wallet teams
integrating the resolver should face no legal friction.

