# From "An Introduction to Counterfactual Regret Minimization" (2013)
# 2.5 RPS Equilibrium

import random

ROCK = 0
PAPER = 1
SCISSORS = 2
NUM_ACTIONS = 3
REGRET_SUM = [[0,0,0], [0,0,0]]
STRATEGY = [[0,0,0], [0,0,0]]
STRATEGY_SUM = [[0,0,0], [0,0,0]]

class RPSTrainer:
    def __init__(self):
        pass

    def get_utility(self, my_action, opp_action):
        if my_action == opp_action: return 0
        if my_action == ROCK and opp_action == SCISSORS: return 1
        if my_action == SCISSORS and opp_action == PAPER: return 1
        if my_action == PAPER and opp_action == ROCK: return 1
        return -1

    def do_one_iteration(self, player):
        self.get_strategy(player)
        my_action = self.get_action(STRATEGY[player])
        opp_action = self.get_action(STRATEGY[1 - player])
        utility = self.get_utility(my_action, opp_action)

        for a in range(NUM_ACTIONS):
            REGRET_SUM[player][a] += self.get_utility(a, opp_action) - utility
        
    # Compute a strategy for a player, then store it in the global variable "STRATEGY"
    def get_strategy(self, player):
        normalizing_sum = 0.0
        for a in range(NUM_ACTIONS):
            STRATEGY[player][a] = REGRET_SUM[player][a] if REGRET_SUM[player][a] > 0 else 0
            normalizing_sum += STRATEGY[player][a]
        
        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                STRATEGY[player][a] /= normalizing_sum
            else:
                STRATEGY[player][a] = 1.0 / NUM_ACTIONS
            STRATEGY_SUM[player][a] += STRATEGY[player][a]
    
    def get_action(self, strategy):
        r = random.random()
        cumulative_probability = 0.0
        a = 0
        while a < NUM_ACTIONS - 1:
            cumulative_probability += strategy[a]
            if r < cumulative_probability: break
            a += 1
        return a
    
    def get_average_strategy(self, player):
        avg_strategy = []
        normalizing_sum = 0.0
        for a in range(NUM_ACTIONS):
            normalizing_sum += STRATEGY_SUM[player][a]
        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                avg_strategy.append(STRATEGY_SUM[player][a] / normalizing_sum)
            else:
                avg_strategy.append(1.0 / normalizing_sum)
        return avg_strategy
    
    def train(self, num_iterations):
        for i in range(num_iterations):
            for player in range(2):
                self.do_one_iteration(player)

def main():
    trainer = RPSTrainer()
    trainer.train(1_000_000)
    for player in range(2):
        print(trainer.get_average_strategy(player))

main()