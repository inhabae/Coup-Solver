# Coup Solver

A Nash equilibrium solver for the 2-player card game [Coup](https://en.wikipedia.org/wiki/Coup_(game)), implemented using **Counterfactual Regret Minimization (CFR)**. The solver converges toward unexploitable strategies by iterating over the full game tree and minimizing regret across all information sets.

---

## Overview

Coup is a 2-player imperfect information card game where each player holds 2 hidden cards (influences) and takes actions by either truthfully or bluffing their card abilities. Players can challenge claimed actions, block actions, or accept them — creating a rich game tree of deception and deduction.

This solver:
- Models the complete 1v1 Coup game tree with full action/response logic
- Runs vanilla CFR to compute approximate Nash equilibrium strategies
- Computes exploitability via best response calculation
- Applies 6 successive tree-pruning rules to reduce the infoset count by **98.8%** (411M → 4.75M at depth 13)

---

## Implementation

### Language & Build

Written in **C++17** with a Python prototype used for initial validation. Built with `make`.

```
make        # builds both solver and test executables
make clean  # removes object files and executables
```

### Key Components

| File | Description |
|---|---|
| `game_state.hpp / .cpp` | Full game state: cards, coins, influence, action history, legal move generation, do/undo mutation |
| `trainer.hpp / .cpp` | CFR training loop, best response calculation, exploitability, tree utilities |
| `solver.cpp` | Entry point — runs training |
| `test.cpp` | Unit tests for game state correctness |

---

## Game Model

### Cards

| Card | Action | Blocking |
|---|---|---|
| Duke | Tax (+3 coins) | Blocks Foreign Aid |
| Captain | Steal (+2 coins from opponent) | Blocks Steal (with Ambassador) |
| Ambassador | — | Blocks Steal (with Captain) |
| Assassin | Assassinate (costs 3 coins) | — |
| Contessa | — | Blocks Assassinate |

### Actions

Every turn a player may take a primary action (Income, Foreign Aid, Tax, Steal, Assassinate, Coup). The opponent may respond by blocking or challenging. Challenges and blocks are recursively handled, with card reveals and influence losses terminating sub-sequences.

The full action enum spans 27 actions including show-card, lose-card, and the special `CLAIM_MATE` endgame move.

### Terminal Conditions

- A player loses both influences → opponent wins
- A player successfully executes `CLAIM_MATE` (deterministic 2v1 endgame) → opponent loses

---

## CFR Algorithm

The solver uses **vanilla CFR** (Zinkevich et al., 2007). At each information set, regrets are accumulated for each action and used to compute a regret-matching strategy. The average strategy over all iterations converges to Nash equilibrium.

```
cfr(game_state, p1_reach, p2_reach):
  if terminal: return utility
  compute strategy via regret matching
  for each action:
    recurse with updated reach probabilities
  update regret sums and strategy sums
```

State is mutated in-place using `do_action` / `undo_action` to avoid per-node memory allocation.

### Infoset Representation

Each information set is identified by:
- The current player's two cards
- The full action history

Hashed via **FNV-1a** for O(1) lookup. Hash collisions are detected and asserted against during training.

---

## Tree Pruning

The game tree is pruned using 6 rules that eliminate strategically dominated or redundant subtrees without affecting Nash equilibrium. Applied during 2v2 subtree traversal:

| # | Rule | Rationale |
|---|---|---|
| 1 | Must Coup when ≥7 coins vs 1-influence opponent | Dominant action — no reason to delay |
| 2 | No Steal/FA/Assassinate after opponent has credibly blocked those | Claimed card history makes those actions inadvisable |
| 3 | No redundant actions when 2v1 Coup-mate is forced | Deterministic endgame, no decision needed |
| 4 | No Tax immediately after Foreign Aid response | Action ordering constraint |
| 5 | No Foreign Aid immediately after Tax response | Action ordering constraint |
| 6 | No dominated weaker actions when opponent allowed a stronger one | Efficiency pruning based on prior acceptance |

These rules reduce the 2v2 infoset count from ~411M to ~4.75M at depth 13.

---

## Exploitability

After training, exploitability is measured by computing each player's **best response** against the other's average strategy:

```
exploitability = BR(opponent's strategy) - Nash value
```

The best response traversal holds one player's strategy fixed, then maximizes over all opponent card distributions weighted by prior probability.

---

## Prototype

The Python prototype in `rps/` implements CFR for **Rock-Paper-Scissors** and a **Kuhn Poker** solver (`kuhn-poker/`) — both used to validate the CFR implementation before building the full Coup solver in C++.

---

## References

- Zinkevich, M., Johanson, M., Bowling, M., Piccione, C. (2007). *Regret Minimization in Games with Incomplete Information*
- Neller, T., Lanctot, M. (2013). *An Introduction to Counterfactual Regret Minimization*
- Bowling, M. et al. (2015). *Heads-up Limit Hold'em Poker Is Solved*
