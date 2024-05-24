from enum import Enum
import numpy as np
from typing import List, Dict
import random 
import pickle 

'''
Kuhn Coup: 
A variant of a popular board game Coup in the format of 1v1.
The deck consists of one Assassin, one Contessa, and one Civilian (no ability).
Each player has one card and starts with one coin.
Players on their turn can:
    1) INCOME to get 1 coin 
    2) ASSASSINATE using 1 coin (with an Assassin card) 
    3) COUP using 2 coins
Players against an ASSASSINATION can:
    1) CHALLENGE 
    2) PASS 
    3) BLOCK ASSASSINATION (with a Contessa card)
At three coins or more, player must Coup.
Whoever eliminates the other player first wins.

This variant was invented to serve as a toy game of Coup. 
'''

class Action(Enum):
    INCOME = 0
    ASSASSINATE = 1
    COUP = 2
    PASS = 3
    CHALLENGE = 4
    BLOCK_ASSASSINATE = 5

class InfoSet():
    def __init__(self, possible_actions):
        self.cumulative_regrets = np.zeros(shape=len(possible_actions))
        self.strategy_sum = np.zeros(shape=len(possible_actions))
        self.num_actions = len(possible_actions)
        self.action_names = [action.value for action in possible_actions]

    def _normalize(self, strategy: np.array) -> np.array:
        '''Normalize a strategy. If there are no positive regrets,
        use a uniform random strategy'''
        if sum(strategy) > 0:
            strategy /= sum(strategy)
        else:
            strategy = np.array([1.0 / self.num_actions] * self.num_actions)
        return strategy
    
    def get_strategy(self, reach_probability: float) -> np.array:
        '''Return regret-matching strategy'''
        strategy = np.maximum(0, self.cumulative_regrets)
        strategy = self._normalize(strategy)
        self.strategy_sum += reach_probability * strategy
        return strategy

    def get_average_strategy(self) -> np.array:
        return self._normalize(self.strategy_sum.copy())
    
class KuhnCoup():
    @staticmethod
    def is_terminal(history: List[Action]) -> bool:
        if len(history) != 0:
            last_action = history[-1]
            if last_action in [Action.COUP, Action.CHALLENGE]:
                return True
            elif last_action == Action.PASS and history[-2] == Action.ASSASSINATE:
                return True
        return False
    
    @staticmethod
    def get_payoff(history: List[Action], cards: List[str], active_player: int) -> int:
        '''Note: Only to be called when is_terminal() is True'''
        last_action = history[-1]
        if last_action == Action.COUP: 
            return -1
        elif last_action == Action.PASS: 
            return 1
        elif last_action == Action.CHALLENGE:
            if history[-2] == Action.BLOCK_ASSASSINATE:
                return 1 if cards[active_player] == 'CONTESSA' else -1
            elif history[-2] == Action.ASSASSINATE:
                return 1 if cards[active_player] == 'ASSASSIN' else -1
            
    
                
    @staticmethod
    def get_possible_actions(history: List[Action], my_coins: int, first_assn: bool) -> List[Action]:
        '''Note: Only to be called when is_terminal() is False'''
        if len(history) == 0:
            return [Action.INCOME, Action.ASSASSINATE]
        
        possible_actions = []
        last_action = history[-1]
        if last_action in [Action.INCOME, Action.PASS]: 
            if my_coins >= 3:
                return [Action.COUP]
            possible_actions.append(Action.INCOME)
            if my_coins > 0:
                possible_actions.append(Action.ASSASSINATE)
            if my_coins > 1:
                possible_actions.append(Action.COUP)
            return possible_actions
        elif last_action == Action.ASSASSINATE: 
            return [Action.PASS, Action.CHALLENGE, Action.BLOCK_ASSASSINATE]
        elif last_action == Action.BLOCK_ASSASSINATE: 
            possible_actions = [Action.PASS, Action.CHALLENGE]

            # If this is player's second assassination, player must challenge opponent's block.
            # This is because not challenging is a dominated strategy.
            # This was implemented to limit the game tree, so that players cannot assassinate and block
            # for an infinite number of times.
            if not first_assn: 
                possible_actions.remove(Action.PASS) 
            return possible_actions    
        
class KuhnCFRTrainer():
    def __init__(self):
        self.infoset_map: Dict[str, InfoSet] = {}

    def get_information_set(self, my_card: str, coins: List[int], history: List[Action], possible_actions: List[Action]) -> InfoSet:
        infoset_key = '' + my_card + str(coins[0]) + str(coins[1])
        infoset_key +=  ''.join([str(action.value) for action in history])

        # Generate a new infoset key if it does not exist
        if infoset_key not in self.infoset_map:
            # Create an infoset 
            self.infoset_map[infoset_key] = InfoSet(possible_actions)   

        return self.infoset_map[infoset_key]
    


    def cfr(self, cards: List[str], coins: List[int], history: List[Action], reach_probabilities: np.array, active_player: int, first_assn_list: List[bool]):
        if KuhnCoup.is_terminal(history):
            return KuhnCoup.get_payoff(history, cards, active_player)
        
        possible_actions = KuhnCoup.get_possible_actions(history, coins[active_player], first_assn_list[active_player])
        my_card = cards[active_player]

        infoset = self.get_information_set(my_card, coins, history, possible_actions)
        strategy = infoset.get_strategy(reach_probabilities[active_player])

        opponent = 1 - active_player

        counterfactual_values = np.zeros(len(possible_actions))

        for ix, action in enumerate(possible_actions):
            action_probability = strategy[ix]
            new_reach_probabilities = reach_probabilities.copy()
            new_reach_probabilities[active_player] *= action_probability

            # Perform actions
            new_coins = coins[:]
            new_history = history[:]
            new_first_assn_list = first_assn_list[:]

            if action == Action.INCOME:
                new_coins[active_player] += 1
            elif action == Action.ASSASSINATE:
                new_coins[active_player] -= 1
                new_first_assn_list[active_player] = False
            elif action == Action.COUP:
                new_coins[active_player] -= 2
            new_history.append(action)

            # Recursively call cfr(), switching the player to act
            counterfactual_values[ix] = -self.cfr(cards, new_coins, new_history, new_reach_probabilities, opponent, new_first_assn_list)

        # Value of the current game state is just counterfactual values weighted by action probabilities
        node_value = counterfactual_values.dot(strategy)

        for ix, action in enumerate(possible_actions):
            regret =  (counterfactual_values[ix] - node_value)
            infoset.cumulative_regrets[ix] += reach_probabilities[opponent] * regret

        return node_value
    
    def train(self, num_iterations: int) -> int:
        util = 0
        kuhn_cards = ['CONTESSA', 'ASSASSIN', 'CIVILIAN']
        for _ in range(num_iterations):
            cards = random.sample(kuhn_cards, 2)
            coins = [1, 1]
            history = []
            reach_probabilities = np.ones(2)
            util += self.cfr(cards, coins, history, reach_probabilities, 0, [0,0])
        return util
    
if __name__ == "__main__":
    num_iterations = 10000
    cfr_trainer = KuhnCFRTrainer()
    print(f"\nRunning Kuhn Coup chance sampling CFR for {num_iterations} iterations")
    util = cfr_trainer.train(num_iterations)
    print(util/num_iterations)
    # solutions:
    # key: history
    # value: strategies for each card holding
    solutions = dict()
    for infoset_key, infoset in cfr_trainer.infoset_map.items():
            card = infoset_key[:8]
            history = infoset_key[10:]

            d = {}
            for i in range(infoset.num_actions):
                d[infoset.action_names[i]] = infoset.get_average_strategy()[i]
            if solutions.get(history):
                solutions[history].append({card : d})
            else:
                solutions[history] = [{card : d}]

    # with open('solutions123.pkl', 'wb') as f:
    #     pickle.dump(solutions, f)

