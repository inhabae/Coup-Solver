'''
Coup:
A popular board game Coup in the format of 1v1.
As of 2024-10-24, 3 additonal Captains replace 3 Ambassadors.
Some moves are limited, based on my assumption that they are not practically never played.
(This was done in order to limit the game tree, due to the limited resources I have.)
'''

import numpy as np
import random 
from enum import Enum
import time
import pickle 

# Each action will be represented by an according integer.
# For the readability of the code, the explicit name of the action is
# used instead of an integer.
class Action(Enum):
    INCOME = 'In'
    ASSASSINATE = 'As'
    COUP = 'Co'
    PASS = 'Pa' # Only passing non-attacks (You cannot pass assassination; You will show a card instead)
    CHALLENGE = 'Ch'
    BLOCK_ASSASSINATE = 'Ba'
    TAX = 'Tx'
    FOREIGN_AID = 'Fa'
    BLOCK_FOREIGN_AID = 'Bf'
    STEAL_1 = 'S1'
    STEAL_2 = 'S2'
    STEAL_0 = 'S0'
    BLOCK_STEAL = 'Bs'
    # SHOW is showing the correct challenged card
    SHOW_CARD = 'Sh'
    # LOSE is losing a card, because you don't have the challenged card
    LOSE_ASSASSIN = 'La'
    LOSE_CAPTAIN = 'Lp'
    LOSE_CONTESSA = 'Lc'
    LOSE_DUKE = 'Ld'

# Infoset is a set of information from a specified player's perspective (regarding the player's
# private card info and the public gamte state info). Infoset holds four things:

# 1. Number of possible actions
# 2. List of the possible actions 
# 3. List of Cumulative regrets, which are used to find the Nash Eq strategy
# 4. List of the Nash Eq. strategies, in probabilities 0 <= p <= 1
class InfoSet():
    def __init__(self, possible_actions: List[str]):
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
    

# Coup has methods for running the game:
# 1. Determining if terminal node
# 2. Determining the payoff at the terminal node
# 3. Get all possible actions given game history
class Coup():
    @staticmethod    

    # NOTE: Dead card marked like:
    # DUKE: DEADDK
    # CAPTAIN: DEADCAPT
    # ASSASSIN: DEADASSA
    # CONTESSA: DEADCONT
    def is_terminal(my_cards, opp_cards) -> bool:
        dead_count = 0
        for card in my_cards:
            if card[:4] == 'DEAD':
                dead_count += 1
        if dead_count == 2: return True

        dead_count = 0
        for card in opp_cards:
            if card[:4] == 'DEAD':
                dead_count += 1
        if dead_count == 2: return True

        return False
    
    @staticmethod
    def get_payoff(my_cards, opp_cards) -> bool:
        dead_count = 0
        for card in my_cards:
            if card[:4] == 'DEAD':
                dead_count += 1
        if dead_count == 2: return -1

        dead_count = 0
        for card in opp_cards:
            if card[:4] == 'DEAD':
                dead_count += 1
        if dead_count == 2: return +1

        print('ERROR: Node was not terminal.')
        raise ValueError

    @staticmethod
    def get_lives(cards: List[str]) -> int:
        lives = 2
        for card in cards:
            if card[:4] == 'DEAD':
                lives -= 1

        return lives

    @staticmethod
    def get_possible_actions(history: List[str], my_coins: int, my_cards: List, opp_lives: int, opp_coins: int) -> List[str]:
        # NOTE: Only to be called when is_terminal() is False

        # steal_value is the stealing action an active player can perform
        # Stealing 0 coins is dominated thus removed from the game tree
        steal_value = []
        if opp_coins >= 2:
            steal_value = [Action.STEAL_2.value]
        elif opp_coins == 1:
            steal_value = [Action.STEAL_1.value]



        if len(history) == 0:
            return [Action.INCOME.value, Action.TAX.value, 
                    Action.STEAL_2.value, Action.FOREIGN_AID.value]     
        
        # For losing a card when losing a life
        possible_losses = []
        if 'CONTESSA' in my_cards: possible_losses.append('Lc')
        if 'ASSASSIN' in my_cards: possible_losses.append('La')
        if 'CAPTAIN' in my_cards: possible_losses.append('Lp')
        if 'DUKE'in my_cards: possible_losses.append('Ld')
    
        possible_actions = []
        last_action = history[-1]

        if last_action == Action.COUP.value:
            return possible_losses

        possible_independent_actions = []
        if my_coins > 10: # 10+ coins, MUST Coup
            possible_independent_actions = [Action.COUP.value]
        # If oppponent has one life, MUST Coup (Dominant Strategy)
        elif opp_lives < 2 and my_coins >= 7:
            possible_independent_actions = [Action.COUP.value]
        # If I have one life and opponent has 7+ coins, MUST STEAL or ASSASSINATE    
        elif len(my_cards) < 2 and opp_coins >= 7:
            if my_coins >= 7:
                possible_independent_actions = [Action.ASSASSINATE.value, Action.COUP.value] + steal_value
            elif my_coins >= 3:
                possible_independent_actions = [Action.ASSASSINATE.value]  + steal_value
            else:
                possible_independent_actions = steal_value
        else:
            possible_independent_actions += [Action.INCOME.value, Action.FOREIGN_AID.value, 
                                            Action.TAX.value]  + steal_value
            if my_coins >= 3:
                possible_independent_actions.append(Action.ASSASSINATE.value)
            if my_coins >= 7:
                possible_independent_actions.append(Action.COUP.value)

        # If last action was independent and unchallengeable, return possible INDEPENDENT actions.
        if last_action in [Action.INCOME.value, Action.PASS.value]: 
            return possible_independent_actions
        # If last action was independent and challengeable, return according blocks and challenge.
        elif last_action == Action.ASSASSINATE.value: 
            if len(my_cards) == 2:
                return [Action.CHALLENGE.value, Action.BLOCK_ASSASSINATE.value] + possible_losses
            # Losing a card when only one life left is dominated.
            else:
                return [Action.CHALLENGE.value, Action.BLOCK_ASSASSINATE.value]
        elif last_action == Action.TAX.value: 
            return [Action.CHALLENGE.value] + possible_independent_actions
        elif last_action in [Action.STEAL_0.value, Action.STEAL_1.value, Action.STEAL_2.value]: 
            return [Action.CHALLENGE.value, Action.BLOCK_STEAL.value] + possible_independent_actions
        elif last_action == Action.FOREIGN_AID.value: 
            return [Action.BLOCK_FOREIGN_AID.value] + possible_independent_actions
        # If last action was a BLOCK, return challenge/pass
        elif last_action in [Action.BLOCK_ASSASSINATE.value, Action.BLOCK_FOREIGN_AID.value, Action.BLOCK_STEAL.value]: 
            return [Action.PASS.value, Action.CHALLENGE.value]
        # If last action was a CHALLENGE, return showing cards

        # TODO: I am assuming showing a card is always better than losing a card.
        # This may not be GTO and will be revisited once I can explore a bigger game tree.
        elif last_action == Action.CHALLENGE.value:
            challenged_action = history[-2]
            if challenged_action in [Action.ASSASSINATE.value] and 'ASSASSIN' in my_cards:
                return [Action.SHOW_CARD.value] # + possible_losses
            elif challenged_action in [Action.TAX.value, Action.BLOCK_FOREIGN_AID.value] and 'DUKE' in my_cards:
                return [Action.SHOW_CARD.value] # + possible_losses
            elif challenged_action in [Action.STEAL_0.value, Action.STEAL_1.value,
                                        Action.STEAL_2.value, Action.BLOCK_STEAL.value] and 'CAPTAIN' in my_cards:
                return [Action.SHOW_CARD.value] # + possible_losses
            elif challenged_action in [Action.BLOCK_ASSASSINATE.value] and 'CONTESSA' in my_cards:
                return [Action.SHOW_CARD.value] # + possible_losses
            else:
                return possible_losses
        # If opponent lost a card, independent turn
        elif last_action in [Action.LOSE_ASSASSIN.value, Action.LOSE_CONTESSA.value,
                             Action.LOSE_CAPTAIN.value, Action.LOSE_DUKE.value]:
            return possible_independent_actions

        # If opponent showed a correct card, MUST lose a card
        elif last_action == Action.SHOW_CARD.value:
            return possible_losses
        
        # Else: return error
        else: 
            print("ERROR: No cases to return possible actions")
            return ValueError
        

        # TODO: how to stop infinite loops ?

# CoupCFRTrainer is to find the Nash Eq strategies through CFR.
class CoupCFRTrainer():
    def __init__(self):
        self.infoset_map = {}
        self.terminal_node_keys = set()

    def get_information_set(self, my_cards: List[str], history: List[str], possible_actions: List[int], active_player: int, initial_cards: List[str]) -> InfoSet:
        # my_cards is alphabetically ordered, only including alive cards.
        card_str = ''
        if active_player == 0:
            card_str += ''.join(initial_cards[:2])
        else:
            card_str += ''.join(initial_cards[:4])

        my_cards = sorted(my_cards)
        if len(my_cards) > 1:
            card_str += my_cards[0] + my_cards[1]
        else:
            card_str += str(my_cards[0])
        infoset_key = '' + card_str + ''.join(history)

        # Generate a new infoset key if it does not exist
        if infoset_key not in self.infoset_map:
            # Create an infoset 
            self.infoset_map[infoset_key] = InfoSet(possible_actions)   

        return self.infoset_map[infoset_key]
    

    # move_counter is for x-move rule where if no lives have been lost for x full turns, its an automatic draw.
    # Taken from 50-move rule from chess, this ensures that the game tree is limited while not disrupting GTO.
    # The best number for x will be tested in some time.
    def cfr(self, initial_cards: List[str], my_cards: List[str], opp_cards: List[str], coins: List[int], 
            history: str, reach_probabilities: np.array, active_player: int, deck: List[str], move_counter):
        # print(len(history))
        # if move_counter >= 8: automatic draw
        if move_counter >= 4:
            return 0

        if Coup.is_terminal(my_cards, opp_cards):
            card_str = ''

            if active_player == 0:
                card_str += ''.join(initial_cards[:2])
            elif active_player == 1:
                card_str += ''.join(initial_cards[-2:])



            my_cards = sorted(my_cards)
            if len(my_cards) > 1:
                card_str += my_cards[0] + my_cards[1]
            else:
                raise ValueError
                card_str += str(my_cards[0])

            self.terminal_node_keys.add(card_str + ''.join(history)) # saving terminal keys for MES calculation purposes
            
            return Coup.get_payoff(my_cards, opp_cards)
        
        
        possible_actions = Coup.get_possible_actions(history, coins[active_player], my_cards, len(opp_cards), coins[1 - active_player])

        infoset = self.get_information_set(my_cards, history, possible_actions, active_player, initial_cards)
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
            new_my_cards = my_cards[:]
            new_opp_cards = opp_cards[:]
            new_deck = deck[:]
            new_move_counter = move_counter

            if action == Action.INCOME.value:
                new_coins[active_player] += 1
            elif action == Action.FOREIGN_AID.value:
                new_coins[active_player] += 2
            elif action == Action.TAX.value:
                new_coins[active_player] += 3
            elif action == Action.COUP.value:
                new_coins[active_player] -= 7
            elif action == Action.ASSASSINATE.value:
                new_coins[active_player] -= 3
            elif action == Action.STEAL_0.value:
                pass
            elif action == Action.STEAL_1.value:
                new_coins[active_player] += 1
                new_coins[opponent] -= 1
            elif action == Action.STEAL_2.value:
                new_coins[active_player] += 2
                new_coins[opponent] -= 2
            elif action == Action.BLOCK_STEAL.value: 
                blocked_steal = history[-1] 
                if blocked_steal == Action.STEAL_1.value:
                    new_coins[active_player] += 1
                    new_coins[opponent] -= 1
                elif blocked_steal == Action.STEAL_2.value:
                    new_coins[active_player] += 2
                    new_coins[opponent] -= 2
            elif action == Action.BLOCK_FOREIGN_AID.value:
                new_coins[opponent] -= 2
            elif action == Action.PASS.value: # PASSING A BLOCK does no impact
                pass
            elif action == Action.CHALLENGE.value: # CHALLENGING leads opponent to show/lose a card.
                pass
            # Put the challenged card back in the deck and draw a random one
            elif action == Action.SHOW_CARD.value:
                challenged_card = ''
                if history[-2] in [Action.ASSASSINATE.value]:
                    challenged_card = 'ASSASSIN'
                elif history[-2] in [Action.STEAL_0.value, Action.STEAL_1.value, 
                                    Action.STEAL_2.value, Action.BLOCK_STEAL.value]:
                    challenged_card = 'CAPTAIN'
                elif history[-2] == Action.BLOCK_ASSASSINATE.value:
                    challenged_card = 'CONTESSA'
                elif history[-2] in [Action.TAX.value, Action.BLOCK_FOREIGN_AID.value]:
                    challenged_card = 'DUKE'
                else:
                    print("Error: Unknown challenged card")
                    raise ValueError
                
                new_my_cards.remove(challenged_card)
                new_deck.append(challenged_card)
                random_index = random.randint(0, len(new_deck) - 1)
                random_card = new_deck.pop(random_index)
                new_my_cards.append(random_card)
            elif action == Action.COUP.value:
                new_coins[active_player] -= 2
            elif action == Action.BLOCK_ASSASSINATE.value:
                pass

            # LOSING A CARD
            elif action in [Action.LOSE_ASSASSIN.value, Action.LOSE_CAPTAIN.value,
                            Action.LOSE_CONTESSA.value, Action.LOSE_DUKE.value]:
                # Challenging an assassinaion wrongly -> two lives loss
                # TODO: need to change action that is appended to history

                # Assassinate -> Challenge -> Show Card -> Lose Two Lives
                if history[-1] == Action.SHOW_CARD.value and history[-3] == Action.ASSASSINATE.value:
                    for card in new_my_cards:
                        if card[:4] != 'DEAD':
                            if card == 'ASSASSIN':
                                card = 'DEADASSA'
                            elif card == 'CONTESSA':
                                card = 'DEADCONT'
                            elif card == 'DUKE':
                                card = 'DEADDK'
                            elif card == 'CAPTAIN':
                                card = 'DEADCAPT'

                # TODO: need to change action that is appended to history
                # Getting a fake contessa claim challenged -> two lives loss
                
                # Assassinate -> Block with Contessa -> Challenge
                elif history[-1] == Action.CHALLENGE.value and history[-2] == Action.BLOCK_ASSASSINATE.value:
                    for card in new_my_cards:
                        if card[:4] != 'DEAD':
                            if card == 'ASSASSIN':
                                card = 'DEADASSA'
                            elif card == 'CONTESSA':
                                card = 'DEADCONT'
                            elif card == 'DUKE':
                                card = 'DEADDK'
                            elif card == 'CAPTAIN':
                                card = 'DEADCAPT'

            # ELSE: Losing a single life
                else:
                    # Lose the selected card.
                    if action == Action.LOSE_ASSASSIN.value:
                        new_my_cards.remove('ASSASSIN')
                        new_my_cards.append('DEADASSA')
                    elif action == Action.LOSE_CAPTAIN.value:
                        new_my_cards.remove('CAPTAIN')
                        new_my_cards.append('DEADCAPT')
                    elif action == Action.LOSE_CONTESSA.value:
                        new_my_cards.remove('CONTESSA')
                        new_my_cards.append('DEADCONT')
                    elif action == Action.LOSE_DUKE.value:
                        new_my_cards.remove('DUKE')
                        new_my_cards.append('DEADDK')

                    # Undo any correctly challenged action
                    if history[-1] == Action.CHALLENGE.value: 
                        if history[-2] == Action.ASSASSINATE.value:
                            new_coins[active_player] += 3
                        elif history[-2] == Action.TAX.value:
                            new_coins[active_player] -= 3
                        elif history[-2] == Action.FOREIGN_AID.value:
                            new_coins[active_player] -= 2
                        elif history[-2] == Action.STEAL_1.value:
                            new_coins[active_player] -= 1
                            new_coins[opponent] += 1
                        elif history[-2] == Action.STEAL_2.value:
                            new_coins[active_player] -= 2
                            new_coins[opponent] += 2
                        elif history[-2] == Action.BLOCK_FOREIGN_AID.value:
                            new_coins[opponent] += 2
                        elif history[-2] == Action.BLOCK_STEAL.value:
                            if history[-3] == Action.STEAL_1.value:
                                new_coins[active_player] -= 1
                                new_coins[opponent] += 1
                            elif history[-3] == Action.STEAL_2.value:
                                new_coins[active_player] -= 2
                                new_coins[opponent] += 2
            
            else:
                print("ERROR: Unknown Action: ", action)
                raise ValueError



            if active_player == 1 and action in [Action.ASSASSINATE.value, Action.INCOME.value, Action.TAX.value,
                                                 Action.STEAL_0.value, Action.STEAL_1.value, Action.STEAL_2.value,
                                                 Action.FOREIGN_AID.value, Action.COUP.value]:
                new_move_counter += 1
            new_history.append(action)


            # Recursively call cfr(), switching the player to act
            # Double turn happens only when you lose a card and then the following turn is yours

            # TODO: TEST THIS CODE
            if action in [Action.LOSE_ASSASSIN.value, Action.LOSE_CAPTAIN.value,
                          Action.LOSE_CONTESSA.value, Action.LOSE_DUKE.value]:
                if len(history) > 3 and history[-1] == Action.SHOW_CARD.value and history[-3] not in [
                Action.BLOCK_ASSASSINATE.value, Action.BLOCK_FOREIGN_AID.value, Action.BLOCK_STEAL.value
                ]:
                    counterfactual_values[ix] = self.cfr(initial_cards, new_my_cards, new_opp_cards, new_coins, new_history, new_reach_probabilities, active_player, new_deck, new_move_counter)
            else:
                counterfactual_values[ix] = -self.cfr(initial_cards, new_opp_cards, new_my_cards, new_coins, new_history, new_reach_probabilities, opponent, new_deck, new_move_counter)

        # Value of the current game state is just counterfactual values weighted by action probabilities
        node_value = counterfactual_values.dot(strategy)

        for ix, action in enumerate(possible_actions):
            regret =  (counterfactual_values[ix] - node_value)
            infoset.cumulative_regrets[ix] += reach_probabilities[opponent] * regret
        return node_value
    
    def train(self, num_iterations: int) -> int:
        util = 0
        
        for i in range(num_iterations):

            deck = ['CONTESSA'] * 3 + ['ASSASSIN'] * 3 +  ['DUKE'] * 3 +  ['CAPTAIN'] * 6

            my_cards = random.sample(deck, 2)
            for card in my_cards:
                deck.remove(card)

            opp_cards = random.sample(deck, 2)
            for card in opp_cards:
                deck.remove(card)

            reach_probabilities = np.ones(2)

            initial_cards = sorted(my_cards[:]) + sorted(opp_cards[:])


            print(initial_cards)


            util += self.cfr(initial_cards, my_cards, opp_cards, [2,2], [], reach_probabilities, 0, deck, 0)
        return util
    
    def _get_possible_actions_from_tree(self, history):
        all_keys = list(self.infoset_map.keys()) + list(self.terminal_node_keys)

        history_keys = []
        for key in all_keys:
            history_starting_index = 0
            for i, char in enumerate(key):
                if char.islower():
                    history_starting_index = i - 1
                    history_keys.append(key[history_starting_index:])
                    break
        
        history_keys = list(set(history_keys))
                
            
        possible_actions = []
        for key in history_keys:
            if len(key) == len(history) + 2 and key[:-2] == history:
                possible_actions.append(key[-2:])
        return list(set(possible_actions))


    # https://aipokertutorial.com/agent-evaluation/ 
    # Note 4.3 
    def brf(self, maximizing_player_card: str, maximizing_player: int, history, opp_reach, depth):
        if Coup.is_terminal(len, opp_lives):
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
        

        # this is for AMBASSASDOR where chances are accounted in the game 
        #elif history % 2 != current_player: # NOT current player's node (NOT his turn to act)
            # pass
            # get possible actions
            # for each action, BRF recursively
            # add EV and return that EV 

        # else if node is PRIVATE CHANCE NODE of CURRENT PLAYER... 
       
        new_opp_reach = opp_reach.copy()
        v = -2
        utils = {}
        w = {}
        for action in self._get_possible_actions_from_tree(history):
            utils[action] = 0
            # w is the total likelihood of opponent taking "action", considering
            # all three cards and its specific strategy with the "action"
            w[action] = 0

        # line 17
        for action in self._get_possible_actions_from_tree(history):
            
            if maximizing_player != len(history) % 2:
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

if __name__ == "__main__":
    num_iterations = 1
    cfr_trainer = CoupCFRTrainer()
    print(f"\nRunning Coup chance sampling CFR for {num_iterations} iterations")



    start_time = time.time()
    util = cfr_trainer.train(num_iterations)
    end_time = time.time()

    print(f"Execution Time: {round(end_time - start_time, 2)} seconds.")

    # print("Approx. EV as approaching NASH EQ:", util/num_iterations)

    # for key in cfr_trainer.terminal_node_keys:
    #     print(key)
    # print(len(cfr_trainer.terminal_node_keys))

    # # For finding best response
    # ev_a = cfr_trainer.brf('CONTESSA', 0, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0.5, 'CONTESSA': 0}, 1)
    # print(f'CONTESSA EV is {ev_a}')
    # ev_b = cfr_trainer.brf('ASSASSIN', 0, '', {'ASSASSIN': 0, 'CIVILIAN': 0.5, 'CONTESSA': 0.5}, 1)
    # print(f'ASSASSIN EV is {ev_b}')
    # ev_c = cfr_trainer.brf('CIVILIAN', 0, '', {'ASSASSIN': 0.5, 'CIVILIAN': 0, 'CONTESSA': 0.5}, 1)
    # print(f'CIVILIAN EV is {ev_c}')

    # total_ev = (ev_a + ev_b + ev_c) / 3

    # print("MES EV FOR P1 IS: ", total_ev)

    # For storing solved data in a file
    # key: history
    # value: strategies for each card holding\
    '''
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

    with open('solutions.pkl', 'wb') as f:
        pickle.dump(solutions, f)


    '''