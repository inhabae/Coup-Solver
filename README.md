# Coup Solver
A solver for the board game "Coup" that employs counterfactual regret minimization (CFR) to determine optimal strategies for a 1v1 format.

![coup zz](https://github.com/user-attachments/assets/fc7a7c7e-344c-4ffd-bcd5-cf6a677fd049)


# How the Solver Works
Counterfactual Regert Minimization (CFR)
* CFR is an algorithm that computes optimal strategies in games with incomplete information by tracking and minimizing regret. It evaluates hypothetical scenarios to refine player decisions, ultimately converging towards Nash equilibrium for optimal gameplay.

Best Response
* Strategies derived from CFR may approach Nash equilibrium but can still be exploitable. A Best Response function calculates exploitability by finding the strategy that maximally exploits current strategies. The function reveals how opponents can take advantage of weaknesses and assesses the exploitability of the current strategy.

# Currently Working On...
* Expansion of the game from the mini version to the full game state.
* Optimization of the game tree to enable fast and practical search by reducing its size.
* An interactive UI to display the strategies.
