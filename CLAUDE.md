# CLAUDE.md

**The architecture documentation for this repository is [`ARCHITECTURE.md`](ARCHITECTURE.md).
Read it before working here.**

It was called `CLAUDE.md` until 2026-09-24, which was the wrong name for it. The file explains
why the settings bar is laid out by pure functions, why a clip asked to stop still counts as
running, why the spin is negated before it is drawn, and what every trap in this codebase has
already cost. None of that is addressed to an assistant — it is addressed to whoever touches the
code next, and the old name hid it from exactly those people.

It stays in the repository rather than moving to `a3-doc` for one reason: it changes in the same
commit as the code it describes. Documentation that ships separately from what it documents drifts,
and this repository has paid for that lesson more than once.

## What goes where

| Kind | Where |
|---|---|
| Architecture, and the reasoning behind it | `ARCHITECTURE.md` (this repo) |
| Build, run, test | `README.md` (this repo) |
| Event flow, button semantics, the settings state machine | `team.md` (this repo) |
| User and technical reference, OSC addresses | [`a3-doc`](https://github.com/a3-audio/a3-doc), published |
| Faults and open work | [GitHub issues](https://github.com/a3-audio/a3-motion-ui/issues) |

There is no wiki, and there should not be one: a wiki is a separate repository that does not move
with the code.
