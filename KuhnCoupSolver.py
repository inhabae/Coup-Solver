'''
Kuhn Coup: 
A variant of a popular board game Coup I created in order to solve the simpler
version of Coup before expanding it to the full game. Below is what is different from the real game.

The deck consists of one Assassin, one Contessa, and one Civilian (who has no ability).

Each player has one card and starts with one coin.

Income: +1 coin
Assassinate: -1 coin (-3 coins in Coup)
Coup: -2 coins (-7 coins in Coup)

With 3 coins+, player must Coup.
'''

import numpy as np
from enum import Enum
from typing import List
import random 
import pickle 
from itertools import permutations

# Each action will be represented by an according integer.
# For the readability of the code, the explicit name of the action is used instead of an integer.
class Action(Enum):
    INCOME = 0
    ASSASSINATE = 1
    COUP = 2
    PASS = 3
    CHALLENGE = 4
    BLOCK_ASSASSINATE = 5

# Infoset is a set of information from a specified player's perspective (regarding the player's
# private card info and the public gamte state info). Infoset holds four things:

# 1. Number of possible actions
# 2. List of the possible actions 
# 3. List of Cumulative regrets, which are used to find the Nash Eq strategy
# 4. List of the Nash Eq. strategies, in probabilities 0 <= p <= 1
class InfoSet():
    def __init__(self, possible_actions):
        self.num_actions = len(possible_actions)
        self.action_names = possible_actions[:]
        self.cumulative_regrets = np.zeros(shape=self.num_actions)
        self.strategy_sum = np.zeros(shape=self.num_actions)

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

# KuhnCoup has methods for running the game:
# 1. Determining if terminal node
# 2. Determining the payoff at the terminal node
# 3. Get all possible actions given game history
class KuhnCoup():
    @staticmethod    
    def is_terminal(history: str) -> bool:
        if len(history) != 0:
            last_action = int(history[-1])
            if last_action in [Action.COUP.value, Action.CHALLENGE.value]:
                return True
            elif last_action == Action.PASS.value and int(history[-2]) == Action.ASSASSINATE.value:
                return True
        return False

    @staticmethod
    def get_payoff(history: str, cards: List[str]) -> int:
        '''Note: Only to be called when is_terminal() is True'''
        last_action = int(history[-1])
        active_player = len(history) % 2

        if last_action == Action.COUP.value: 
            return -1
        elif last_action == Action.PASS.value: 
            return 1
        elif last_action == Action.CHALLENGE.value:
            if int(history[-2]) == Action.BLOCK_ASSASSINATE.value:
                return 1 if cards[active_player] == 'CONTESSA' else -1
            elif int(history[-2]) == Action.ASSASSINATE.value:
                return 1 if cards[active_player] == 'ASSASSIN' else -1

    @staticmethod
    def get_possible_actions(history: str, my_coins: int, first_assn: bool) -> List[int]:
        '''Note: Only to be called when is_terminal() is False'''
        if len(history) == 0:
            return [Action.INCOME.value, Action.ASSASSINATE.value]
        
        possible_actions = []
        last_action = int(history[-1])
        if last_action in [Action.INCOME.value, Action.PASS.value]: 
            if my_coins >= 3:
                return [Action.COUP.value]
            possible_actions.append(Action.INCOME.value)
            if my_coins > 0:
                possible_actions.append(Action.ASSASSINATE.value)
            if my_coins > 1:
                possible_actions.append(Action.COUP.value)
            return possible_actions
        elif last_action == Action.ASSASSINATE.value: 
            return [Action.PASS.value, Action.CHALLENGE.value, Action.BLOCK_ASSASSINATE.value]
        elif last_action == Action.BLOCK_ASSASSINATE.value: 
            possible_actions = [Action.PASS.value, Action.CHALLENGE.value]

            # If this is player's second assassination, player must challenge opponent's block.
            # This is because not challenging is a dominated strategy.
            # This was implemented to limit the game tree, so that players cannot assassinate and block
            # for an infinite number of times.
            if not first_assn: 
                possible_actions.remove(Action.PASS.value) 
            return possible_actions    
    
# KuhnCFRTrainer finds the Nash Eq strategies through CFR.
class KuhnCFRTrainer():
    def __init__(self):
        self.infoset_map = {}
        self.terminal_node_keys = set()

    def get_information_set(self, my_card: str, history: str, possible_actions: List[int]) -> InfoSet:
        infoset_key = '' + my_card + ''.join([action for action in history])
        # Generate a new infoset key if it does not exist
        if infoset_key not in self.infoset_map:
            self.infoset_map[infoset_key] = InfoSet(possible_actions)   
        return self.infoset_map[infoset_key]
    
    def cfr(self, cards: List[str], coins: List[int], history: str, reach_probabilities: np.array, active_player: int, first_assn_list: List[bool]):
        if KuhnCoup.is_terminal(history):
            self.terminal_node_keys.add(cards[active_player] + ''.join([history])) # saving terminal keys for MES calculation purposes
            return KuhnCoup.get_payoff(history, cards)
        
        possible_actions = KuhnCoup.get_possible_actions(history, coins[active_player], first_assn_list[active_player])
        my_card = cards[active_player]
        infoset = self.get_information_set(my_card, history, possible_actions)
        strategy = infoset.get_strategy(reach_probabilities[active_player])
        opponent = 1 - active_player
        counterfactual_values = np.zeros(len(possible_actions))
        for ix, action in enumerate(possible_actions):
            action_probability = strategy[ix]
            new_reach_probabilities = reach_probabilities.copy()
            new_reach_probabilities[active_player] *= action_probability
            # Perform actions
            new_coins = coins[:]
            new_history = history
            new_first_assn_list = first_assn_list[:]
            if action == Action.INCOME.value:
                new_coins[active_player] += 1
            elif action == Action.ASSASSINATE.value:
                new_coins[active_player] -= 1
                new_first_assn_list[active_player] = False
            elif action == Action.COUP.value:
                new_coins[active_player] -= 2
            new_history += str(action)
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
            history = ''
            reach_probabilities = np.ones(2)
            util += self.cfr(cards, coins, history, reach_probabilities, 0, [True, True])
        return util
    
    def calculate_strategy_ev(self, p1_card, p2_card, history, node_probability):
        if KuhnCoup.is_terminal(history):
            last_action = int(history[-1])
            if last_action == Action.COUP.value: 
                return -1
            elif last_action == Action.PASS.value: # Passing an assassination loses.
                return 1
            challenged_player = len(history) % 2
            if challenged_player == 0:
                if int(history[-2]) == Action.BLOCK_ASSASSINATE.value:
                    return 1 if p1_card == 'CONTESSA' else -1
                elif int(history[-2]) == Action.ASSASSINATE.value:
                    return 1 if p1_card == 'ASSASSIN' else -1
            elif challenged_player == 1:
                if int(history[-2]) == Action.BLOCK_ASSASSINATE.value:
                    return 1 if p2_card == 'CONTESSA' else -1
                elif int(history[-2]) == Action.ASSASSINATE.value:
                    return 1 if p2_card == 'ASSASSIN' else -1
                
        ev = 0
        for action in self._get_possible_actions_from_tree(history):
            card = p1_card
            if len(history) % 2 == 1:
                card = p2_card
            action_index = self.infoset_map[card+history].action_names.index(int(action))
            strategy = self.infoset_map[card + history].get_average_strategy()[action_index]
            new_node_probability = node_probability * strategy
            ev += -self.calculate_strategy_ev(p1_card, p2_card, history + action, new_node_probability) * new_node_probability
        return ev
    
    def _get_possible_actions_from_tree(self, history):
        all_keys = list(self.infoset_map.keys()) + list(self.terminal_node_keys)
        all_keys = list(set(key[8:] for key in all_keys))
        possible_actions = []
        for key in all_keys:
            if len(key) == len(history) + 1 and key[:-1] == history:
                possible_actions.append(key[-1])
        return list(set(possible_actions))

    # https://aipokertutorial.com/agent-evaluation/ 
    # Note 4.3 
    def brf(self, maximizing_player_card: str, maximizing_player: int, history, opp_reach, depth):
        if KuhnCoup.is_terminal(history):
            last_action = int(history[-1])
            if last_action == Action.COUP.value: 
                return -1
            elif last_action == Action.PASS.value: # Passing an assassination is game-losing.
                return 1
            # Challenging depends on opponent reach probability. 
            challenged_player = len(history) % 2

            if challenged_player == maximizing_player:
                if int(history[-2]) == Action.BLOCK_ASSASSINATE.value:
                    return 1 if maximizing_player_card == 'CONTESSA' else -1
                elif int(history[-2]) == Action.ASSASSINATE.value:
                    return 1 if maximizing_player_card == 'ASSASSIN' else -1
                
            elif challenged_player != maximizing_player:
                total_opp_reach = 0
                for i in ['ASSASSIN', 'CIVILIAN', 'CONTESSA']:
                    total_opp_reach += opp_reach[i]
                opp_normalized = dict()
                for i in ['ASSASSIN', 'CIVILIAN', 'CONTESSA']: # opp reach of each card
                    opp_normalized[i] = opp_reach[i] / total_opp_reach
                if int(history[-2]) == Action.BLOCK_ASSASSINATE.value:
                    return (2 * opp_normalized['CONTESSA'] - 1)
                elif int(history[-2]) == Action.ASSASSINATE.value:
                    return (2 * opp_normalized['ASSASSIN'] - 1)
        new_opp_reach = opp_reach.copy()
        v = -2
        utils = {}
        w = {}
        for action in self._get_possible_actions_from_tree(history):
            utils[action] = 0
            # w is the total likelihood of opponent taking "action", considering
            # all three cards and its specific strategy with the "action"
            w[action] = 0
        for action in self._get_possible_actions_from_tree(history):
            if maximizing_player != len(history) % 2:
                for card in ['ASSASSIN', 'CIVILIAN', 'CONTESSA']:
                    # NOTE: With a small number of iterations, it is possible to run into
                    # an unexplored node of the game tree.
                    action_index = self.infoset_map[card+history].action_names.index(int(action))
                    strategy = self.infoset_map[card + history].get_average_strategy()[action_index]

                    new_opp_reach[card] = opp_reach[card] * strategy
                    w[action] += new_opp_reach[card]
            utils[action] = -self.brf(maximizing_player_card, maximizing_player, history + action, new_opp_reach, depth+1)
            if maximizing_player == len(history) % 2 and utils[action] > v:
                v = utils[action]
        if maximizing_player != len(history) % 2:
            v = 0
            for action in self._get_possible_actions_from_tree(history):
               v += (w[action] / sum(w.values())) * utils[action]
        return v
    
    def calculate_exploitability(self):
        # Payoff of P1 comparing p1 average strategy and p2 average strategy (as it converges to Nash)
        p1_p2_payoff = 0
        for cards in list(permutations(['ASSASSIN', 'CIVILIAN', 'CONTESSA'], 2)):
            p1_p2_payoff += self.calculate_strategy_ev(cards[0], cards[1], '', 1) / 6

        ev_1a = cfr_trainer.brf('CONTESSA', 0, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0.5, 'CONTESSA': 0}, 1)
        ev_1b = cfr_trainer.brf('ASSASSIN', 0, '', {'ASSASSIN': 0, 'CIVILIAN': 0.5, 'CONTESSA': 0.5}, 1)
        ev_1c = cfr_trainer.brf('CIVILIAN', 0, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0, 'CONTESSA': 0.5}, 1)
        p2_payoff_vs_p1_br = -(ev_1a + ev_1b + ev_1c) / 3
        ev_2a = -cfr_trainer.brf('CONTESSA', 1, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0.5, 'CONTESSA': 0}, 1)
        ev_2b = -cfr_trainer.brf('ASSASSIN', 1, '', {'ASSASSIN': 0, 'CIVILIAN': 0.5, 'CONTESSA': 0.5}, 1)
        ev_2c = -cfr_trainer.brf('CIVILIAN', 1, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0, 'CONTESSA': 0.5}, 1)
        p1_payoff_vs_p2_br = -(ev_2a + ev_2b + ev_2c) / 3

        # Exploitability:
        exp = (p1_p2_payoff - p1_payoff_vs_p2_br) + (-p1_p2_payoff - p2_payoff_vs_p1_br)
        return exp

if __name__ == "__main__":
    cfr_trainer = KuhnCFRTrainer()
    utils = 0
    iters = 0
    while True:
        try:
            target_exp = float(input("Target Exploitability (0 < exploitability < 1): "))
            if target_exp >= 1 or target_exp <= 0:
                raise ValueError
            break
        except ValueError:
            print("Invalid input.\n")

    exp = 1_000_000
    while exp > target_exp:
        utils += cfr_trainer.train(100_000)
        iters += 100_000

        exp = cfr_trainer.calculate_exploitability()
        print(f"Iter: {iters}: CURRENT EXPLOITABILITY: ", exp, "STRATEGY EV: ", utils/iters)

    # Storing the solutions in a pickle file
    solutions = dict()
    for infoset_key, infoset in cfr_trainer.infoset_map.items():
        card = infoset_key[:8]
        history = infoset_key[8:]

        d = {}
        for i in range(infoset.num_actions):
            d[infoset.action_names[i]] = infoset.get_average_strategy()[i]
        if solutions.get(history):
            solutions[history].append({card : d})
        else:
            solutions[history] = [{card : d}]

    file_name = input("Name of the file for the solutions: ")

    with open(f'{file_name}.pkl', 'wb') as f:
        pickle.dump(solutions, f)