# 01 — Team

Two people, one company: **Simplify it S.R.L.** (Bolivia).

## Grover Hernando Menacho Quisbert — Founder, lead developer

<https://www.linkedin.com/in/groustuff/>

Owner of Simplify it S.R.L. and the person who writes the contract. **Building systems since 2010
(16 years), 13 of them in ERP** — implementation, custom development, and migrations, for clients
who run their businesses on the result.

Owns the contract, the QPI implementation, testing, technical decisions, and wallet outreach.
**20 hours per week** committed to QHandle.

## Silvia Mariela Vargas Cordova — QA and business analyst

<https://www.linkedin.com/in/silvia-vargas-4213a8311/>

Owns test coverage, acceptance criteria against the milestone definitions, and wallet-integration
requirements gathering in M1.

Worth stating: a naming service that resolves payment addresses is a product where a wrong answer
costs someone money. Having QA as a named role rather than an afterthought is why the validation
layer has 28 dedicated assertions covering homograph, case-aliasing and embedded-NUL attacks before
anything else was built. **20 hours per week** committed to QHandle.

## Founder–market fit

**We are the user.** The founder holds an ENS name on every network where he transacts — except
Qubic, among the most important of them. That is n=1 and not presented as market research; it is
why the project exists ([05](05-validation-traction.md)).

**What we bring is delivery, not crypto-native credentials.** We have not shipped a blockchain
product. We have shipped enterprise software for sixteen years to clients who depend on it. The
evidence that this transfers is in this repository, produced before any funding was requested: a
~1,500-line QPI contract compiling against real core headers, 81 passing assertions, measured state
layout, and a design record citing every decision to core source — including three bugs found in
self-review and documented rather than hidden.

One piece of that experience is unusually on-point. Simplify it's day job is **custom Odoo
development and migrations** — moving live production data between schema versions without losing
or corrupting it. The single largest technical risk in QHandle (**R1**, in
[10](10-risks-security.md)) is exactly a migration problem: whether a `MIGRATE` procedure can
re-insert ~105,000 records into a resized hash map in one invocation. The platforms could hardly be
more different, and we are not claiming the skills port directly. But it is why we recognised that
risk at design time and put it in milestone 1 instead of discovering it after the audit.

## Where we are weak

- **No prior crypto project.** First blockchain product for both of us.
- **No relationship with any of the four target wallets.** M1's conversations are cold outreach —
  which is why they are M1, gated at 11.5% of budget.
- **Small team, part-time.** 20 hours per week each — 40 combined. The milestone plan is sized to
  that and front-loads the decisions that could kill the project.

## Ownership

Grover Menacho has final authority on scope. Silvia Vargas owns acceptance: a milestone is not done
until it passes her criteria, which are the ones written into [09](09-milestones-budget.md).

## Past work

**Simplify it S.R.L. — <https://www.simplifyit.com.bo>** · custom Odoo development, integrations
and migrations.

No prior blockchain project, stated plainly. What is on record is sixteen years of shipping
business systems that clients depend on daily — which is the relevant question here, since QHandle
is infrastructure that has to keep working rather than a product that has to go viral.

## Contact

gm@simplifyit.com.bo
