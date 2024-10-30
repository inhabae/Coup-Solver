BLOCKS = ['Ba', 'Bs', 'Bf']
EOT_ACTIONS = ["La", "Lc", "Lp", "Ld", "Pa", "In"] # End of Turn Actions

class Coup():
    @staticmethod    
    def is_terminal(my_cards, opp_cards):
        if my_cards.count('0') == 2 or opp_cards.count('0') == 2: return True
        return False

    @staticmethod
    def get_payoff(my_cards, opp_cards) -> bool:
        if my_cards.count('0') == 2: return -1
        if opp_cards.count('0') == 2: return 1

        raise ValueError("get_payoff() called at a non-terminal node.")

    @staticmethod
    def get_num_lives(cards):
        lives = 2 - cards.count('0')
        if lives > 3 or lives < 0: raise ValueError(f"Invalid number of lives: {lives}.")
        return lives
    
    @staticmethod
    def get_alive_cards(cards):
        alive_cards = [cards[0], cards[1]]
        for i in range(2, len(cards), 2):
            if cards[i+1] == '0':
                alive_cards.remove(cards[i])
            else:
                alive_cards.remove(cards[i])
                alive_cards.append(cards[i+1])
        return sorted(alive_cards)
    
    @staticmethod
    def get_lose_actions(cards):
        alive_cards = [cards[0], cards[1]]
        for i in range(2, len(cards), 2):
            if cards[i+1] == '0':
                alive_cards.remove(cards[i])
            else:
                alive_cards.remove(cards[i])
                alive_cards.append(cards[i+1])
        card_to_action = {'1': "La", '2': "Lp", '3': "Lc", '4': "Ld"}
        return [card_to_action[card] for card in set(alive_cards)]

    @staticmethod
    def has_action_card(action, cards):
        alive_cards = [cards[0], cards[1]]
        for i in range(2, len(cards), 2):
            if cards[i+1] == '0':
                alive_cards.remove(cards[i])
            else:
                alive_cards.remove(cards[i])
                alive_cards.append(cards[i+1])
        action_to_card = {"As": '1', "S1": '2', "S2": '2', "Bs": '2', "Ba": '3', "Bf": '4', "Tx": '4'}
        return action_to_card.get(action) in alive_cards
    
    @staticmethod
    def _get_possible_opening_moves(my_cards, opp_cards, my_coins, opp_coins):
        my_coins, opp_coins = int(my_coins), int(opp_coins)

        # Mandatory Coup
        if my_coins > 10: 
            return ['Co']
        
        # Forced win
        if my_coins >= 7 and Coup.get_num_lives(opp_cards) == 1:
            return ['Co']
    
        # Configuring the correct steal action (S1 or S2)
        steal_action = []
        if opp_coins == 1:
            steal_action = ['S1']
        elif opp_coins >= 2:
            steal_action = ['S2']

        # 1v2
        if Coup.get_num_lives(opp_cards) - Coup.get_num_lives(my_cards) == 1:
            # Forced loss, limit the game tree
            if opp_coins >= 9:
                return ['In']
            # Must steal vs 7-8 coins
            if opp_coins >= 7:
                return ['S2']

            if my_coins < 3:
                return ['Tx'] + steal_action
            elif my_coins < 6:
                return ['Tx', 'As'] + steal_action
            elif my_coins >= 7 and opp_coins < 3:
                return ['Tx', 'As', 'Co']
    
        # 1v1 Situation
        if Coup.get_num_lives(my_cards) * Coup.get_num_lives(opp_cards) == 1:
            # Forced plays
            if my_coins >= 3 and opp_coins >= 9:
                return ['As']
            elif my_coins >= 3 and opp_coins >= 7:
                return ['As', 'S2']
            elif opp_coins >= 7:
                return ['S2']
            if my_coins < 3:
                return ['In', 'Fa', 'Tx'] + steal_action
            return ['In', 'Fa', 'Tx', 'As'] + steal_action

        # All others (2v1 or 2v2)
        if my_coins >= 7:
            return ['In', 'Tx', 'As', 'Co']
        elif my_coins >= 3:
            return ['In', 'Tx', 'As']
        return ['In', 'Tx']

    @staticmethod
    # NOTE: Should be called on a non-terminal node.
    def get_possible_actions(history, my_cards, opp_cards, my_coins, opp_coins):
        if not history: # First turn of the game
            return ["In", "Tx"] 
        
        last_action = history[-1]
        # vs End of Turn action
        if last_action in EOT_ACTIONS:
            return Coup._get_possible_opening_moves(my_cards, opp_cards, my_coins, opp_coins)
        # vs Successful attack
        elif last_action in ['Co', 'Sc']:
            return Coup.get_lose_actions(my_cards)
        # vs Blocks
        elif last_action in BLOCKS:
            return ['Ch', 'Pa']
        # vs Challenge
        elif last_action == 'Ch':
            if Coup.has_action_card(history[-2], my_cards): 
                return ['Sc']
            return Coup.get_lose_actions(my_cards)
        # vs Tax
        elif last_action == 'Tx':
            return Coup._get_possible_opening_moves(my_cards, opp_cards, my_coins, opp_coins) + ['Ch']
        elif last_action in ['S1', 'S2']:
            return Coup._get_possible_opening_moves(my_cards, opp_cards, my_coins, opp_coins) + ['Bs', 'Ch'] 
        # vs Foreign aid
        elif last_action == 'Fa':
            return Coup._get_possible_opening_moves(my_cards, opp_cards, my_coins, opp_coins) + ['Bf']
        # vs Assassinate
        elif last_action == 'As':
            # Always block when holding Contessa
            if Coup.has_action_card('Ba', my_cards):
                return ['Ba'] 
            # Never lose a card when it's last life
            if Coup.get_num_lives(my_cards) == 1:
                return ['Ba', 'Ch']
            return ['Ba', 'Ch'] + Coup.get_lose_actions(my_cards) 
        
        raise ValueError("All situations are expected to be resolved. This code should be unreachable.")