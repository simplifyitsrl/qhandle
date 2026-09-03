# 06 — Market, GTM, competition

## Bottom-up market logic

No TAM slide. The addressable market is bounded by something countable:

```
Addressable = Qubic addresses that publish an address and receive from strangers
Reachable   = those whose wallet surfaces name resolution
Paying      = those who renew after year one
```

The second line is the binding constraint, and it is why wallet integration — not end-user
marketing — is the entire GTM. A handle is worthless if the person paying you cannot type it.

> **TODO — founder input.** Counts for the top line: active Qubic addresses, and any figure for
> addresses receiving from multiple senders. Per the metrics rule this needs a period, a
> denominator and a source — on-chain or explorer data. If not obtainable, we say so rather than
> guess.

## Alternatives today

| Alternative | Why insufficient |
|---|---|
| Copy-paste the raw address | The status quo. Cannot be spoken, printed or verified by eye — and is what address-poisoning exploits |
| Wallet-local address books | Private per wallet, non-portable, useless for a *first* payment — the highest-risk case |
| Address in a bio / QR | No indirection: rotating keys means re-publishing everywhere, and stale copies circulate |
| A centralised directory | Requires trusting an operator; no wallet will hardcode a resolver that can rug the mapping |
| Do nothing | The honest baseline. Qubic has functioned without this. The bet is that address friction caps merchant and retail adoption |

## Competition

**A prior attempt exists, and we are not going to pretend otherwise.** A naming-service project
was started on Qubic, made no visible progress, and has since been removed from Qubic's project
list. Reviewers will know this better than we do, so it belongs here rather than in a footnote.

Two readings, and we think both are true:

- **It is evidence, not a warning.** Someone else independently identified the same gap. Demand for
  a Qubic naming service is not a hypothesis we invented.
- **The interesting question is why it stalled**, and we would rather learn that than guess. If it
  stalled on the platform constraints described in [03](03-why-now-why-qubic.md) — the
  `issueAsset()` 7-character cap forcing a custom registry, and static memory making capacity a
  permanent, pre-committed decision — then the design work already in this repository is precisely
  the obstacle it did not clear. If it stalled for team or funding reasons, that is a different
  lesson and it changes nothing about the technical approach.

> **TODO — founder input.** Name of the prior project, a link if one survives, and anything known
> about why it stopped. Do not speculate: if we do not know, the proposal should say we do not know
> and ask Qubic during evaluation.

**Indirect competition:** a wallet shipping its own private naming feature. The credible threat, and
an explicit M1 question. **Reference points** — ENS, ADA Handle, SNS — show the category is real and
that recurring-renewal pricing works at scale; we do not claim their numbers transfer.

## Acquisition channel

**Primary: wallet integrations. Owner: Grover Menacho (founder)** — the conversations are technical
(what it takes to surface a third-party resolver in a send flow) and a founder opens more doors in
cold outreach.

| Target | Why |
|---|---|
| HASHWallet | Hardware wallet; address verification on a small screen is exactly the pain |
| Rubic | Established Qubic wallet with an existing send flow |
| Qubihub | Ecosystem hub, plausible discovery surface |
| Qubic-Connect (MetaMask Snap) | Reaches users already habituated to ENS names |

**Cold outreach, stated plainly: we have no existing relationship with any of the four.** A genuine
weakness, and why these conversations are milestone 1, gated at 11.5% of the budget, rather than
assumed. M1 establishes who will actually surface a resolver; M3 delivers one live integration plus
a guide for the rest — one working integration is worth more than four conversations in progress.

**Secondary: direct to repeat receivers** via Qubic community channels and the launch AMA. This only
works *after* a wallet resolves names; pitching earlier sells something users cannot use.

**Explicitly not doing:** paid acquisition, airdrops, or registration-incentive campaigns — they
manufacture exactly the vanity metric the program screens out.

## First 10–100 users

**First ~10:** the team, the partner wallet's team, community members at launch — infrastructure
validation, not demand. **Next ~100:** repeat receivers via the partner's release notes and the AMA.
That cohort is the real signal, judged on how many **renew** after a year.
