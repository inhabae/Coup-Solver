# From "An Introduction to Counterfactual Regret Minimization" (2013)
# 2.4 Rock-Paper-Scissors 

import random

ROCK = 0
PAPER = 1
SCISSORS = 2
NUM_ACTIONS = 3
REGRET_SUM = [0,0,0]
STRATEGY = [0,0,0]
STRATEGY_SUM = [0,0,0]
OPP_STRATEGY = [0.3, 0.5, 0.2]

class RPSTrainer:
    def __init__(self):
        pass

    def get_utility(self, action1, action2):
        if action1 == action2: return 0
        if action1 == ROCK and action2 == SCISSORS: return 1
        if action1 == SCISSORS and action2 == PAPER: return 1
        if action1 == PAPER and action2 == ROCK: return 1
        return -1

    def do_one_iteration(self):
        self.get_strategy()
        my_action = self.get_action(STRATEGY)
        opp_action = self.get_action(OPP_STRATEGY)
        utility = self.get_utility(my_action, opp_action)

        for a in range(NUM_ACTIONS):
            REGRET_SUM[a] += self.get_utility(a, opp_action) - utility
        
    def get_strategy(self):
        normalizing_sum = 0.0
        for a in range(NUM_ACTIONS):
            STRATEGY[a] = REGRET_SUM[a] if REGRET_SUM[a] > 0 else 0
            normalizing_sum += STRATEGY[a]
        
        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                STRATEGY[a] /= normalizing_sum
            else:
                STRATEGY[a] = 1.0 / NUM_ACTIONS
            STRATEGY_SUM[a] += STRATEGY[a]
    
    def get_action(self, strategy):
        r = random.random()
        cumulative_probability = 0.0
        a = 0
        while a < NUM_ACTIONS - 1:
            cumulative_probability += strategy[a]
            if r < cumulative_probability: break
            a += 1
        return a
    
    def get_average_strategy(self):
        avg_strategy = []
        normalizing_sum = 0.0
        for a in range(NUM_ACTIONS):
            normalizing_sum += STRATEGY_SUM[a]
        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                avg_strategy.append(STRATEGY_SUM[a] / normalizing_sum)
            else:
                avg_strategy.append(1.0 / normalizing_sum)
        return avg_strategy
    
    def train(self, num_iterations):
        for i in range(num_iterations):
            self.do_one_iteration()
    
def main():
    trainer = RPSTrainer()
    trainer.train(1_000_000)
    print(trainer.get_average_strategy())

main()