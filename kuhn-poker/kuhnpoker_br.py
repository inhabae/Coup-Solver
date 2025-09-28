import random

PASS = 0
BET = 1
NUM_ACTIONS = 2

node_map = dict()

class Node():
    def __init__(self, infoset_string):
        self.infoset_string = infoset_string
        self.regret_sum = [0] * NUM_ACTIONS
        self.strategy = [0] * NUM_ACTIONS
        self.strategy_sum = [0] * NUM_ACTIONS
    
    def get_strategy(self, realization_weight):
        normalizing_sum = 0
        for a in range(NUM_ACTIONS):
            if self.regret_sum[a] > 0:
                self.strategy[a] = self.regret_sum[a]  
            else:
                self.strategy[a] = 0
            normalizing_sum += self.strategy[a]

        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                self.strategy[a] /= normalizing_sum
            else:
                self.strategy[a] = 1.0 / NUM_ACTIONS
            self.strategy_sum[a] += realization_weight * self.strategy[a]
        return self.strategy
    
    def get_average_strategy(self):
        average_strategy = [0] * NUM_ACTIONS
        normalizing_sum = 0
        for a in range(NUM_ACTIONS):
            normalizing_sum += self.strategy_sum[a]
        for a in range(NUM_ACTIONS):
            if normalizing_sum > 0:
                average_strategy[a] = self.strategy_sum[a] / normalizing_sum
            else:
                average_strategy[a] = 1.0 / NUM_ACTIONS
        return average_strategy
    
    def __str__(self):
        return f"{self.infoset_string:>4}: {self.get_average_strategy()}"

            
def initialize_infosets():
    for card in [1,2,3]:
        for h in ["", "b", "p", "pb"]:
            infoset_string = str(card)+h
            node_map[infoset_string] = Node(infoset_string)

def is_terminal(h):
    return h in ["pp", "bb", "pbb", "pbp", "bp"]

def get_utility(cards, history):
    plays = len(history)
    player = plays % 2
    opp = 1 - player

    if plays > 1:
        terminal_pass = history[plays - 1] == "p"
        double_bet = history[plays - 2:plays] == "bb"
        is_player_card_higher = cards[player] > cards[opp]
    
        if terminal_pass:
            if history == "pp":
                return 1 if is_player_card_higher else -1
            else:
                return 1
        elif double_bet:
            return 2 if is_player_card_higher else -2


def cfr(cards, history, reach_p1, reach_p2):
    plays = len(history)
    player = plays % 2
    opp = 1 - player

    # return payoff for terminal states
    if is_terminal(history):
        return get_utility(cards, history)

    infoset_string = str(cards[player]) + history
    # get infoset node
    infoset_node = node_map[infoset_string]
    # for each action, call cfr recursviely
    strategy = infoset_node.get_strategy(reach_p1 if player == 0 else reach_p2)
    util = [0] * NUM_ACTIONS
    node_utility = 0

    for a in range(NUM_ACTIONS):
        next_history = history + ("p" if a == 0 else "b")
        util[a] = (
            -cfr(cards, next_history, reach_p1 * strategy[a], reach_p2)
            if player == 0
            else -cfr(cards, next_history, reach_p1, reach_p2 * strategy[a]))
        node_utility += strategy[a] * util[a]
    # for each acation, compute and accumulate cfr
    for a in range(NUM_ACTIONS):
        regret = util[a] - node_utility
        infoset_node.regret_sum[a] += (reach_p2 if player == 0 else reach_p1) * regret
    return node_utility

# Pre-shuffling, rather than handling inside CFR => Chance-Sampling
def train(iterations):
    cards = [1,2,3]
    util = 0.0
    for i in range(iterations):
        # Fisher–Yates shuffle
        for c1 in range(len(cards) - 1, 0, -1):
            c2 = random.randint(0, c1)
            cards[c1], cards[c2] = cards[c2], cards[c1] 
        util += cfr(cards, "", 1, 1)
    print("Average game value: ", util / iterations)

    for node in node_map.values():
        print(node)

    return (util / iterations)

def get_utility_br(my_card, opp_cards, history, card_distribution):
    plays = len(history)
    player = plays % 2

    if plays > 1:
        terminal_pass = history[plays - 1] == "p"
        double_bet = history[plays - 2:plays] == "bb"
    
        # Opponent folded
        if terminal_pass and history != "pp":
            return 1
        # Double bet
        elif double_bet:
            # normalize card distribution
            total_probs = sum(card_distribution)
            if total_probs == 0: return 0 # This node utility is not considered, so it doesnt matter what we return
            for i in range(len(card_distribution)):
                card_distribution[i] /= total_probs

            utility = 0.0
            for i in range(len(opp_cards)):
                if opp_cards[i] < my_card:
                    utility += card_distribution[i] * 2
                else:
                    utility -= card_distribution[i] * 2
            if player != 1: utility *= -1
            return utility
        # pass/pass
        else: 
            # normalize card distribution
            total_probs = sum(card_distribution)
            if total_probs == 0: return 0 # This node utility is not considered, so it doesnt matter what we return
            for i in range(len(card_distribution)):
                card_distribution[i] /= total_probs

            utility = 0.0
            for i in range(len(opp_cards)):
                if opp_cards[i] < my_card:
                    utility += card_distribution[i]
                else:
                    utility -= card_distribution[i]
            if player != 1: utility *= -1
            return utility

def calculate_p2_br(p2_card, p1_cards, history, card_distribution):
    if is_terminal(history):
        u = get_utility_br(p2_card, p1_cards, history, card_distribution)
        return u
    
    if len(history) % 2 == 1: # p2's node
        best_value = -100
        best_action = -1
        for a in range(NUM_ACTIONS):
            next_history = history + ("p" if a == 0 else "b")
            utility = -calculate_p2_br(p2_card, p1_cards, next_history, card_distribution)
            if utility > best_value:
                best_value = utility
                best_action = a
        return best_value
    else:
        action_probs = [0.0] * NUM_ACTIONS # new_reach_prob
        new_card_distribution = [[] for _ in range(NUM_ACTIONS)]
        for a in range(NUM_ACTIONS):
            for c in range(len(p1_cards)):
                infoset_node = node_map[str(p1_cards[c]) + history]
                v = infoset_node.get_average_strategy()[a] * card_distribution[c]
                action_probs[a] += v
                new_card_distribution[a].append(v)

        node_utility = 0.0
        for a in range(NUM_ACTIONS):
            next_history = history + ("p" if a == 0 else "b")
            action_utility = -calculate_p2_br(p2_card, p1_cards, next_history, new_card_distribution[a])
            node_utility += action_utility * action_probs[a]
        return node_utility
    
def main():
    initialize_infosets()
    iterations = 1_000_000
    train(iterations)

    ### CUSTOM STRATEGY
    global node_map

    # node_map["1"].strategy_sum = [1/3, 2/3]
    # node_map["1"].strategy_sum = [2/3, 1/3]
    # node_map["1pb"].strategy_sum = [1.0, 0.0]
    # node_map["2"].strategy_sum = [1.0, 0.0]
    # node_map["2pb"].strategy_sum = [1/3, 2/3]
    # node_map["3"].strategy_sum = [0.0, 1.0]
    # node_map["3pb"].strategy_sum = [0.0, 1.0]


    br_utility = 0.0
    for c in [1,2,3]:
        opp_cards = [1,2,3]
        opp_cards.remove(c)
        card_utility = calculate_p2_br(c, opp_cards, "", [0.5, 0.5])
        print(f"Card {c}: Utility: {card_utility}")
        br_utility += card_utility
    
    print("Player 2 BR Utility: ", br_utility / 3)


main()
