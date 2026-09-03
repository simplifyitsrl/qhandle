# 02 — Problem, ICP, desirability

## The pain

A Qubic address is a 60-character string. Three concrete failures follow:

1. **Verification burden.** Users compare first and last characters and hope — the behaviour
   address-poisoning attacks exploit by seeding history with an address whose visible ends match.
2. **Out-of-band transmission.** Merchants and creators publish addresses in bios, invoices and
   chat. No format survives being spoken, printed, or read off a screen.
3. **No stable identity.** Rotating keys means re-publishing everywhere; there is no indirection
   between "who I am" and "which key I currently use."

QHandle's `owner`/`target` split addresses (3) directly: the owner repoints the handle without the
name changing.

## Primary ICP — wallet developers (the integration buyer)

Teams shipping **HASHWallet, Rubic, Qubihub, Qubic-Connect**. Address entry is the highest-friction
step in their send flow and a recurring support burden. Today they solve it with local address
books and copy-paste — private per wallet, non-portable, and useless for a *first* payment to
someone new, which is the highest-risk case.

What they need is a resolve call that is free, fast, and cannot fail — which is what QHandle
provides: resolution is a `PUBLIC_FUNCTION`, so it costs nothing and keeps serving **even when the
contract's fee reserve is depleted**. A wallet integrating QHandle cannot be broken by QHandle's own
economics.

## Secondary ICP — repeat receivers (the paying customer)

Holders who receive from many counterparties and therefore have reason to pay for a name others
type: merchants accepting QUBIC, creators receiving tips, OTC desks publishing a settlement
address, projects publishing a treasury address.

The willingness-to-pay hypothesis is narrow and testable: *someone who publishes their address
publicly and receives from strangers will pay a small annual fee for a name they can say out loud.*

**Not the ICP:** users who transact once, and speculators buying names to resell — length-tiered
pricing and annual renewal make bulk squatting expensive rather than attractive.

## Desirability evidence

Pre-build/prototype. We have a clear ICP, a reproducible prototype artifact, and a concrete
why-Qubic argument. We do **not** have user interviews, and we will not dress desk research up as
validation. What partly substitutes is that the primary ICP is four *named teams*, contactable
directly with a verifiable outcome — see [05](05-validation-traction.md).

## What would make us abandon this

If wallet teams will not surface a third-party resolver, the primary distribution channel is gone
and the secondary ICP alone does not justify the build. If a wallet reveals an equivalent feature
already shipping, the land-grab argument collapses. Both are answerable in conversation before most
of the budget is spent — which is why they are milestone 1.
