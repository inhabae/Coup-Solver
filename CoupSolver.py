'''
Coup:
A popular board game Coup in the format of 1v1.
As of 2024-10-24, 3 additonal Captains replace 3 Ambassadors.
Some moves are limited, based on my assumption that they are not practically never played.
(This was done in order to limit the game tree, due to the limited resources I have.)
'''

import numpy as np
import random 
import time
import csv
from collections import deque # For game tree construction
from Coup import Coup
from BrowseSolution import display_solution

class InfoSet():
    def __init__(self, possible_actions):
        self.num_actions = len(possible_actions)
        self.action_names = possible_actions[:]
        self.cumulative_regrets = np.zeros(shape=self.num_actions)
        self.strategy_sum = np.zeros(shape=self.num_actions)

    def get_num_actions(self):
        return self.num_actions

    def _normalize(self, strategy: np.array) -> np.array:
        '''Normalize a strategy.'''
        if sum(strategy) > 0:
            strategy /= sum(strategy)
        else:
            strategy = np.array([1.0 / self.num_actions] * self.num_actions)
        return strategy
    
    def get_strategy(self, reach_probability: float) -> np.array:
        '''Return a regret-matching strategy'''
        strategy = np.maximum(0, self.cumulative_regrets)
        strategy = self._normalize(strategy)
        self.strategy_sum += reach_probability * strategy
        return strategy

    def get_average_strategy(self) -> np.array:
        return self._normalize(self.strategy_sum.copy())
    
    def print_average_strategy(self):
        avg_strategy = self.get_average_strategy()
        for i in range(self.num_actions):
            print(self.action_names[i], ": ", avg_strategy[i], end=" ")
    
MAX_TURN_ALLOWED = 5

class CoupCFRTrainer():
    def __init__(self):
        self.infoset_map = {}
        # self.terminal_infosets = set()
        self.game_tree = {"" : {'11', '12', '13', '14', '22', '23', '24', '33', '34', '44'}} 

    # def store_terminal_infosets(self, my_cards, history):
    #     infoset_key = my_cards + ''.join(history)
    #     self.terminal_infosets.add(infoset_key)

    def get_information_set(self, my_cards, history, possible_actions):
        infoset_key = my_cards + ''.join(history)
        # Generate a new infoset key if it does not exist
        if infoset_key not in self.infoset_map:
            self.infoset_map[infoset_key] = InfoSet(possible_actions)   
        return self.infoset_map[infoset_key]
    
    def cfr(self, history, my_cards, opp_cards, my_coins, opp_coins, reach_probs, active_player, deck, opening_player, turn_counter):
        if Coup.is_terminal(my_cards, opp_cards):
            # self.store_terminal_infosets(my_cards, history) # Storing a terminal infoset key
            self.game_tree[my_cards + ''.join(history)] = set()
            return Coup.get_payoff(my_cards, opp_cards)
        
        # Turn counter is to limit the game tree, based on the assumption that 
        # average Coup games end fairly quickly (in ~5 turns).
        if turn_counter >= MAX_TURN_ALLOWED:
            self.game_tree[my_cards + ''.join(history)] = set()
            return 0
        
        all_possible_actions = Coup.get_possible_actions(history, my_cards, opp_cards, my_coins, opp_coins)
        current_infoset = self.get_information_set(my_cards, history, all_possible_actions)
        strategy = current_infoset.get_strategy(reach_probs[active_player])
        opponent = 1 - active_player
        counterfactual_values = np.zeros(len(all_possible_actions))

        for ix, action in enumerate(all_possible_actions):
            
            action_probability = strategy[ix]
            new_reach_probs = reach_probs.copy()
            new_reach_probs[active_player] *= action_probability

            # This separates variables from other actions. Each opening player and turn counter are specific
            # to the action chosen.
            new_opening_player = opening_player
            new_turn_counter = turn_counter

            # Performing an action
            new_history = history[:]
            new_my_cards = my_cards
            new_opp_cards = opp_cards
            new_my_coins = my_coins
            new_opp_coins = opp_coins
            new_deck = deck[:]

            # Coin-affecting actions
            if action == "In": new_my_coins += 1
            elif action == "Fa": new_my_coins += 2
            elif action == "Tx": new_my_coins += 3
            elif action == "Co": new_my_coins -= 7
            elif action == "As": new_my_coins -= 3
            elif action == "S1":
                new_my_coins += 1
                new_opp_coins -= 1
            elif action == "S2":
                new_my_coins += 2
                new_opp_coins -= 2
            elif action == "Bs": 
                steal = history[-1] 
                if steal == "S1":
                    new_my_coins += 1
                    new_opp_coins -= 1
                elif steal == "S2":
                    new_my_coins += 2
                    new_opp_coins -= 2
                else:
                    raise ValueError(f"Invalid steal value: {steal}")
            elif action == "Bf":
                new_opp_coins -= 2
            # Showing a challenged card will be shuffled and drawn.
            elif action == "Sc":
                action_to_card = {"As": '1', "S1": '2', "S2": '2', "Bs": '2', "Ba": '3', "Bf": '4', "Tx": '4'}
                challenged_card = action_to_card[history[-2]] 
                new_deck.append(challenged_card)
                drawn_card = new_deck.pop(random.randint(0, len(new_deck) - 1))
                new_my_cards += challenged_card + drawn_card
            # Card-losing actions
            elif action in ['La', 'Lp', 'Lc', 'Ld']:
                # Double Assassination
                # Case 1: Assassinate -> Challenge -> Show Challenged Card -> Lose Two Lives
                if Coup.get_num_lives(my_cards) == 2 and history[-1] == 'Sc' and history[-3] == 'As':
                    for card in Coup.get_alive_cards(my_cards):
                        new_my_cards += card + '0'
                # Case 2: Assassinate -> Block Assassination without Contessa-> Challenge 
                elif Coup.get_num_lives(my_cards) == 2 and history[-1] == 'Ch' and history[-2] == 'Ba':
                    for card in Coup.get_alive_cards(my_cards):
                        new_my_cards += card + '0'
                # Losing one life
                else:
                    action_to_card = {'La': '1', 'Lp': '2', 'Lc': '3', 'Ld': '4'}
                    new_my_cards += action_to_card[action] + '0'
                    # Undo a failed action
                    if history[-1] == 'Ch':
                        if history[-2] == 'As':
                            new_my_coins += 3
                        elif history[-2] == 'Tx':
                            new_my_coins -= 3
                        elif history[-2] == 'Fa':
                            new_my_coins -= 2
                        elif history[-2] == "S1" or (history[-2] == 'Bs' and history[-3] == 'S1'):
                            new_my_coins -= 1
                            new_opp_coins += 1
                        elif history[-2] == "S2" or (history[-2] == 'Bs' and history[-3] == 'S2'):
                            new_my_coins -= 2
                            new_opp_coins += 2
                        elif history[-2] == "Bf":
                            new_opp_coins += 2
            new_history.append(action)

            # Determining an opening player (used for double turn)
            if action in {"In", "Fa", "Tx", "S2", "S1", "As", "Co"}: 
                new_opening_player = active_player
                if active_player == 1: 
                    new_turn_counter += 1

            # Losing a card on enemy's turn -> Turn should not be swapped.
            if action in ['La', 'Lp', 'Lc', 'Ld'] and active_player != new_opening_player:
                self.game_tree.setdefault(my_cards + ''.join(history), set()).add(new_my_cards + ''.join(new_history))
                counterfactual_values[ix] = self.cfr(new_history, new_my_cards, new_opp_cards, new_my_coins, new_opp_coins, new_reach_probs, active_player, new_deck, new_opening_player, new_turn_counter)
            else:
                self.game_tree.setdefault(my_cards + ''.join(history), set()).add(opp_cards + ''.join(new_history))
                counterfactual_values[ix] = -self.cfr(new_history, new_opp_cards, new_my_cards, new_opp_coins, new_my_coins, new_reach_probs, opponent, new_deck, new_opening_player, new_turn_counter)
        
        # Value of the current game state is just counterfactual values weighted by action probabilities
        node_value = counterfactual_values.dot(strategy)
        for ix, action in enumerate(all_possible_actions):
            regret =  (counterfactual_values[ix] - node_value)
            current_infoset.cumulative_regrets[ix] += reach_probs[opponent] * regret
        return node_value
    
    def train(self, num_iterations: int) -> int:
        util = 0
        for _ in range(num_iterations):

            # NOTE: The code below randomizes cards. It is commented to test display_solution for now.
            # deck = ['1'] * 3 + ['2'] * 6 +  ['3'] * 3 +  ['4'] * 3
            # my_cards = ''.join(random.sample(deck, 2))
            # for card in my_cards:
            #     deck.remove(card)
            # opp_cards = ''.join(random.sample(deck, 2))
            # for card in opp_cards:
            #     deck.remove(card)
            reach_probabilities = np.ones(2)


            deck = deck = ['1'] * 1 + ['2'] * 6 +  ['3'] * 1 +  ['4'] * 3
            my_cards = '11'
            opp_cards = '33'
            util += self.cfr([], my_cards, opp_cards, 2, 2, reach_probabilities, 0, deck, 0, 0)
        return util
    
    def _get_possible_actions_from_tree(self, history):
        all_keys = list(self.infoset_map.keys()) + list(self.terminal_infosets)
        history_keys = set()
        for key in all_keys:
            for i, char in enumerate(key):
                if char.islower():
                    history_keys.add(key[i-1:])
                    break
        possible_actions = []
        for key in history_keys:
            if len(key) == len(history) + 2 and key[:-2] == history:
                possible_actions.append(key[-2:])
        return list(set(possible_actions))

''' 
    # NOTE: Incomplete BR implementation.
    def is_brf_terminal(self, cards, history):
        return cards + ''.join(history) in list(self.infoset_map.keys()) + list(self.terminal_infosets)   

    # https://aipokertutorial.com/agent-evaluation/ 
    # Note 4.3 
    def brf(self, maximizing_cards, maximizing_player, history, opp_reach, depth, active_player, lives):
        # If terminal, return the BRF payoff
        if self.is_brf_terminal(maximizing_cards, history, lives):
            return -1 if lives[0] == 0 else 1
        
        # Check the website above to implement chance nodes here. (For AMBASSASDOR)

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
            if maximizing_player != active_player:
                for card in ['ASSASSIN', 'CIVILIAN', 'CONTESSA']:

                    # NOTE: With a small number of iterations, it is possible to run into
                    # an unexplored node of the game tree.

                    action_index = self.infoset_map[card+history].action_names.index(int(action))
                    strategy = self.infoset_map[card + history].get_average_strategy()[action_index]

                    new_opp_reach[card] = opp_reach[card] * strategy
                    w[action] += new_opp_reach[card]
                
            # RECURSION
            utils[action] = -self.brf(maximizing_player_card, maximizing_player, history + action, new_opp_reach, depth+1)
        
            if maximizing_player == len(history) % 2 and utils[action] > v:
                v = utils[action]

        if maximizing_player != len(history) % 2:
            v = 0
            for action in self._get_possible_actions_from_tree(history):
               v += (w[action] / sum(w.values())) * utils[action]
        return v
'''

if __name__ == "__main__":
    num_iterations = int(input("# of CFR iterations: "))
    cfr_trainer = CoupCFRTrainer()
    print(f"\nRunning Coup chance sampling CFR for {num_iterations} iterations")
    start_time = time.time()
    util = cfr_trainer.train(num_iterations)
    end_time = time.time()
    print(f"Execution Time: {round(end_time - start_time, 2)} seconds.")
    
    # display_solution(cfr_trainer.infoset_map, cfr_trainer.game_tree)
    # print(cfr_trainer.game_tree)

    # solutions = cfr_trainer.infoset_map

    # start_time = time.time()
    # with open('solutions.csv', 'w') as f:
    #     writer = csv.writer(f)
    #     writer.writerow(['Key', 'Value'])
    #     for k, v in solutions.items():
    #         s = ""
    #         for i in range(v.num_actions):
    #             s += v.action_names[i] + ": " + str(v.get_average_strategy()[i]) + ", "
    #         writer.writerow([k, s])

    # end_time = time.time()
    # print(f"Storing Time: {round(end_time - start_time, 2)} seconds.")