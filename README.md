# Coup Solver
The Coup Solver is designed for the board game "Coup," using Counterfactual Regret Minimization (CFR) to converge towards Nash equilibrium strategies in a 1v1 format.


# How the Solver Works
Counterfactual Regert Minimization (CFR)
* CFR is an algorithm that computes strategies for games with incomplete information. It minimizes regret by evaluating hypothetical scenarios, allowing players to refine their decisions over time. Through repeated iterations, the algorithm converges toward Nash equilibrium, providing a solid framework for effective gameplay in uncertain situations.
  
Best Response
* While CFR-derived strategies can approach Nash equilibrium, they may still be exploitable. The Best Response function determines the strategy that can maximally exploit current approaches. This highlights weaknesses in the opponent's strategy and assesses the potential exploitability of the current plan.

# History
* Initially developed as a Kuhn Coup Solver, it solved a simplified version of Coup where each player had one card. The deck consisted of one Assassin, one Contessa, and one Civilian (the latter having no special ability).
* The solver has since been expanded to accommodate the full game of Coup, although the Ambassador implementation has not yet been included.

# Limitations and Resolutions
Huge game tree size
* Coup has an extremely big game tree— a player has five or more possible actions per turn (Income, Foreign Aid, Tax, Steal, Exchange) + (Assassinate and/or Coup), while the opposing player also has more than one response (Challenge, Block, or Pass). With each turn the game tree grows exponentially, making the solution of the full game tree almost impossible.

One resolution: Turn Counter
* A turn counter is implemented to declare a draw after a specific number of full-turns, similar to the 50-move rule in chess. This discourages players with advantages from prolonging the game unnecessarily and helps prevent infinite loops in the game tree. (e.g. one player repeatedly using Foreign Aid and the other Stealing)

Another resolution: Manual Branching
* To further reduce the game tree, certain actions are removed at some points in the game. For example, it is unlikely (and dubious) for a human player to use Foreign Aid in the early game. Additionally, once a win is guaranteed, only those winning moves are played. Conversely, moves that would result in a guaranteed loss are avoided entirely.

Long iteration time
* Even after all efforts, each iteration takes over a minute when the turn counter is set to 5. (Note that the turn counter of 5 is inadequate, as games can last much longer.) Considering that convergence may require millions of iterations of CFR—which would exceed 700 days—this approach is simply not feasible.

# What now?
Full Coup implementation
* Implement Ambassador to the game

Manual game tree construction
* Though compromising some accuracy, it signficantly reduces the game tree size by focusing on moves commonly played by human players. For instance, Incoming 5 times and Assassinating is removed from the game tree. Much more likely is to Taxing twice or Incoming once with the intention to Assassinate.

An interactive UI to display the strategies (like GTOWizard)
