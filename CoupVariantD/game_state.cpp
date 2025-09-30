#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

// For print_game_state()
const std::unordered_map<Action, std::string> action_to_string = {
    {INCOME, "INCOME"},
    {FOREIGN_AID, "FOREIGN_AID"},
    {TAX, "TAX"},
    {STEAL1, "STEAL1"},
    {STEAL2, "STEAL2"},
    {ASSASSINATE, "ASSASSINATE"},
    {COUP, "COUP"},
    {BLOCK_FOREIGN_AID, "BLOCK_FOREIGN_AID"},
    {BLOCK_STEAL1_AMB, "BLOCK_STEAL1_AMB"},
    {BLOCK_STEAL2_AMB, "BLOCK_STEAL2_AMB"},
    {BLOCK_STEAL1_CAP, "BLOCK_STEAL1_CAP"},
    {BLOCK_STEAL2_CAP, "BLOCK_STEAL2_CAP"},
    {BLOCK_ASSASSINATE, "BLOCK_ASSASSINATE"},
    {CHALLENGE, "CHALLENGE"},
    {PASS_BLOCK, "PASS_BLOCK"},
    {SHOW_ASSASSIN, "SHOW_ASSASSIN"},
    {SHOW_AMBASSADOR, "SHOW_AMBASSADOR"},
    {SHOW_CAPTAIN, "SHOW_CAPTAIN"},
    {SHOW_CONTESSA, "SHOW_CONTESSA"},
    {SHOW_DUKE, "SHOW_DUKE"},
    {LOSE_ASSASSIN, "LOSE_ASSASSIN"},
    {LOSE_AMBASSADOR, "LOSE_AMBASSADOR"},
    {LOSE_CAPTAIN, "LOSE_CAPTAIN"},
    {LOSE_CONTESSA, "LOSE_CONTESSA"},
    {LOSE_DUKE, "LOSE_DUKE"},
    {LOSE_BOTH, "LOSE_BOTH"}
};

GameState::GameState() {
    current_player = 0;
    p1_cards = {ASSASSIN, ASSASSIN};
    p2_cards = {ASSASSIN, ASSASSIN};
    p1_influence = {1,1};
    p2_influence = {1,1};
    p1_coins = 2;
    p2_coins = 2;
    p1_num_assassinate_blocked = 0;
    p2_num_assassinate_blocked = 0;
    p1_num_steal_blocked = 0;
    p2_num_steal_blocked = 0;
    p1_num_fa_blocked = 0;
    p2_num_fa_blocked = 0;
    history = {};
}

bool GameState::is_terminal() const {
    if (p1_influence[0] == 0 && p1_influence[1] == 0) return true;
    if (p2_influence[0] == 0 && p2_influence[1] == 0) return true;
    return false;
}

void GameState::set_cards(Card p1_card1, Card p1_card2, Card p2_card1, Card p2_card2) {
    // Arranging cards in order, for infoset consistency
    if (p1_card1 <= p1_card2) p1_cards = {p1_card1, p1_card2};
    else p1_cards = {p1_card2, p1_card1};
    if (p2_card1 <= p2_card2) p2_cards = {p2_card1, p2_card2};
    else p2_cards = {p2_card2, p2_card1};
}

// For best response function
void GameState::set_my_cards(std::array<Card, 2> cards) {
    if (current_player == 0) p1_cards = cards;
    else p2_cards = cards;
}

double GameState::get_utility() const {
    if (current_player == 0) {
        if (p1_influence[0] == 0 && p1_influence[1] == 0) return -1.0;
        if (p2_influence[0] == 0 && p2_influence[1] == 0) return 1.0;
    }
    else {
        if (p1_influence[0] == 0 && p1_influence[1] == 0) return 1.0;
        if (p2_influence[0] == 0 && p2_influence[1] == 0) return -1.0;
    }
    assert(false && "Invalid state in get_utility()");
}
   
// double GameState::get_br_utility(int maximizing_player, std::vector<double> card_distribution) const {
//     Action last_action = history.back();
//     // Coup
//     if (last_action == COUP) return -1.0;
//     // Maximizing Player is challenged -> Check card 
//     if (maximizing_player == current_player) {
//         Action challenged_action = history[history.size() - 2];
//         Card current_card = (current_player == 0) ? p1_card : p2_card;
//         bool has_correct_card = false;
//         if (current_card == DUKE) {
//             if (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID) {
//                 has_correct_card = true;
//             }
//         }
//         else if (challenged_action == ASSASSINATE && current_card == ASSASSIN) {
//             has_correct_card = true;
//         }
//         else if (challenged_action == BLOCK_ASSASSINATE && current_card == CONTESSA) {
//             has_correct_card = true;
//         }
//         else if (current_card == CAPTAIN) {
//             if (challenged_action == STEAL1 || challenged_action == STEAL2 || challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP) {
//                 has_correct_card = true;
//             }
//         }
//         else if (current_card == AMBASSADOR) {
//             if (challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL2_AMB) {
//                 has_correct_card = true;
//             }
//         }
//         return has_correct_card ? 1.0 : -1.0;
//     }
//     // Non-Maximizing Player is challenged -> Consider distribution
//     else {
//         // Find challenged action
//         Action challenged_action = history[history.size() - 2];

//         Card challenged_card = ASSASSIN;
//         if (challenged_action == BLOCK_ASSASSINATE) challenged_card = CONTESSA;
//         else if (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID) challenged_card = DUKE;
//         else if (challenged_action == STEAL1 || challenged_action == STEAL2) challenged_card = CAPTAIN;
//         else if (challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP) challenged_card = CAPTAIN;
//         else if (challenged_action == BLOCK_STEAL1_AMB|| challenged_action == BLOCK_STEAL2_AMB) challenged_card = AMBASSADOR;
        
//         std::vector<Card> cards {ASSASSIN, AMBASSADOR, CAPTAIN, CONTESSA, DUKE};

//         // Calculate utility based on the distribution
//         double utility = 0.0;
//         for (size_t c = 0; c < card_distribution.size(); c++)
//             if (challenged_card == cards[c]) {
//                 utility += card_distribution[c] * 1.0;
//             }
//             else {
//                 utility -= card_distribution[c] * 1.0;
//             } 
//         return utility;
//     }
//     assert(false && "get_br_utility() called on non-terminal state");
// }

int GameState::get_current_player() const {
    return current_player;
}

std::vector<Action> GameState::get_legal_actions() const {
    // Terminal: No more actions
    if (is_terminal()) return {};

    // Game start
    if (history.size() == 0) return {INCOME, FOREIGN_AID, TAX, STEAL2};

    Action last_action = history.back();
    std::vector<Action> legal_actions = {INCOME, FOREIGN_AID, TAX};
    int current_player_coins = (current_player == 0) ? p1_coins : p2_coins;
    int opp_coins = (current_player == 0) ? p2_coins : p1_coins;
    bool current_player_was_assassinate_blocked = (current_player == 0) ? p1_num_assassinate_blocked >= 2: p2_num_assassinate_blocked >= 2;
    bool current_player_was_steal_blocked = (current_player == 0) ? p1_num_steal_blocked >= 2 : p2_num_steal_blocked >= 2;
    bool current_player_was_fa_blocked = (current_player == 0) ? p1_num_fa_blocked >= 2 : p2_num_fa_blocked >= 2;
    std::array<int, 2> my_influence = (current_player == 0) ? p1_influence : p2_influence;
    std::array<int, 2> opp_influence = (current_player == 0) ? p2_influence : p1_influence;
    int my_num_lives = my_influence[0] + my_influence[1];
    int opp_num_lives = opp_influence[0] + opp_influence[1];
    
    // vs Income, Pass_block, Lose_card
    if (last_action == INCOME || last_action == PASS_BLOCK || (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE)) {
        if (current_player_coins >= COIN_TO_MUST_COUP) return {COUP};
        if (current_player_coins >= COIN_TO_COUP && opp_num_lives == 1) return {COUP};
        if (opp_coins == 1) legal_actions.push_back(STEAL1);
        else if (opp_coins >= 2) legal_actions.push_back(STEAL2);
        if (current_player_coins >= COIN_TO_ASSASSINATE) legal_actions.push_back(ASSASSINATE);
        if (current_player_coins >= COIN_TO_COUP) legal_actions.push_back(COUP);
        return legal_actions;
    }
    // vs Foreign_aid
    else if (last_action == FOREIGN_AID) {
        if (current_player_coins >= COIN_TO_MUST_COUP) return {COUP};
        if (current_player_coins >= COIN_TO_COUP && opp_num_lives == 1) return {COUP};
        legal_actions.push_back(STEAL2);
        if (current_player_coins >= COIN_TO_ASSASSINATE) legal_actions.push_back(ASSASSINATE);
        if (current_player_coins >= COIN_TO_COUP) legal_actions.push_back(COUP);
        legal_actions.push_back(BLOCK_FOREIGN_AID);
        return legal_actions;
    }
    // vs Tax
    else if (last_action == TAX) {
        if (current_player_coins >= COIN_TO_MUST_COUP) return {COUP};
        if (current_player_coins >= COIN_TO_COUP && opp_num_lives == 1) return {COUP};
        legal_actions.push_back(STEAL2);
        if (current_player_coins >= COIN_TO_ASSASSINATE) legal_actions.push_back(ASSASSINATE);
        if (current_player_coins >= COIN_TO_COUP) legal_actions.push_back(COUP);
        legal_actions.push_back(CHALLENGE);
        return legal_actions;
    }
    // vs Steal 1
    else if (last_action == STEAL1) {
        return {TAX, BLOCK_STEAL1_AMB, BLOCK_STEAL1_AMB, CHALLENGE};
    }
    // vs Steal 2
    else if (last_action == STEAL2) {
        if (current_player_coins >= COIN_TO_MUST_COUP) return {COUP};
        if (current_player_coins >= COIN_TO_COUP && opp_num_lives == 1) return {COUP};
        legal_actions.clear();
        legal_actions.push_back(TAX);
        if (current_player_coins >= COIN_TO_ASSASSINATE) legal_actions.push_back(ASSASSINATE);
        if (current_player_coins >= COIN_TO_COUP) legal_actions.push_back(COUP);
        legal_actions.push_back(BLOCK_STEAL2_AMB);
        legal_actions.push_back(BLOCK_STEAL2_CAP);
        legal_actions.push_back(CHALLENGE);
        return legal_actions;
    }
    // vs Assassinate
    else if (last_action == ASSASSINATE) {
        // One life: Must BLOCK or CHALLENGE
        if (my_num_lives == 1) {
            return {BLOCK_ASSASSINATE, CHALLENGE};
        }
        legal_actions = {BLOCK_ASSASSINATE, CHALLENGE};
        std::array<Card, 2> current_cards = (current_player == 0) ? p1_cards : p2_cards;
        std::vector<Action> lose_actions = get_card_losing_actions(current_cards, my_influence);
        legal_actions.insert(legal_actions.end(), lose_actions.begin(), lose_actions.end());
        return legal_actions;
    }
    // vs Block_foreign_aid
    else if (last_action == BLOCK_FOREIGN_AID) {
        if (current_player_was_fa_blocked) return {CHALLENGE};
        return {CHALLENGE, PASS_BLOCK};
    }
    // vs Block_steal
    else if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB || last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) {
        if (current_player_was_steal_blocked) return {CHALLENGE};
        return {CHALLENGE, PASS_BLOCK};
    }
    // vs Block_assassinate
    else if (last_action == BLOCK_ASSASSINATE) {
        if (current_player_was_assassinate_blocked) return {CHALLENGE};
        return {CHALLENGE, PASS_BLOCK};
    }
    // vs Coup
    else if (last_action == COUP) {
        std::array<Card, 2> current_cards = (current_player == 0) ? p1_cards : p2_cards;
        std::array<int, 2> my_influence = (current_player == 0) ? p1_influence : p2_influence;
        return get_card_losing_actions(current_cards, my_influence);
    }
    // vs Challenge
    else if (last_action == CHALLENGE) {
        Action challenged_action = history[history.size() - 2];
        std::array<Card, 2> current_cards = (current_player == 0) ? p1_cards : p2_cards;
        for (int i = 0; i < 2; i++) {
            if (my_influence[i] == 0) continue;
            Card current_card = current_cards[i];
            if (current_card == DUKE) {
                if (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID) {
                    return {SHOW_DUKE};
                }
            }
            else if (challenged_action == ASSASSINATE && current_card == ASSASSIN) {
                return {SHOW_ASSASSIN};
            }
            else if (challenged_action == BLOCK_ASSASSINATE && current_card == CONTESSA) {
                return {SHOW_CONTESSA};
            }
            else if (current_card == CAPTAIN) {
                if (challenged_action == STEAL1 || challenged_action == STEAL2 || challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP) {
                    return {SHOW_CAPTAIN};
                }
            }
            else if (current_card == AMBASSADOR) {
                if (challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL2_AMB) {
                    return {SHOW_AMBASSADOR};
                }
            }
        }

        // Check for double assassination
        if (my_num_lives == 2 && challenged_action == BLOCK_ASSASSINATE) return {LOSE_BOTH};
        return get_card_losing_actions(current_cards, my_influence);
    }
    // vs Show_card
    else if (last_action >= SHOW_ASSASSIN && last_action <= SHOW_DUKE) {
        // Check for double assassination
        if (my_num_lives == 2 && last_action == SHOW_ASSASSIN) return {LOSE_BOTH};
        std::array<Card, 2> current_cards = (current_player == 0) ? p1_cards : p2_cards;
        return get_card_losing_actions(current_cards, my_influence);
    }
    assert(false && "Invalid state in get_legal_actions()");
}

std::vector<Action> GameState::get_card_losing_actions(const std::array <Card, 2> player_cards, const std::array<int, 2> player_influence) const {
    std::vector<Action> legal_actions = {};

    // Losing Card 1
    if (player_influence[0] == 1) {
        if (player_cards[0] == ASSASSIN) legal_actions.push_back(LOSE_ASSASSIN);
        else if (player_cards[0] == AMBASSADOR) legal_actions.push_back(LOSE_AMBASSADOR);
        else if (player_cards[0] == CAPTAIN) legal_actions.push_back(LOSE_CAPTAIN);
        else if (player_cards[0] == CONTESSA) legal_actions.push_back(LOSE_CONTESSA);
        else if (player_cards[0] == DUKE) legal_actions.push_back(LOSE_DUKE);
    }

    // Losing Card 2
    // Case 1: Same card, but Card 1 is dead
    // Case 2: Different card
    if (player_influence[1] == 1) {
        if ((player_cards[0] == player_cards[1] && player_influence[0] == 0) ||
            (player_cards[0] != player_cards[1])) {
            if (player_cards[1] == ASSASSIN) legal_actions.push_back(LOSE_ASSASSIN);
            else if (player_cards[1] == AMBASSADOR) legal_actions.push_back(LOSE_AMBASSADOR);
            else if (player_cards[1] == CAPTAIN) legal_actions.push_back(LOSE_CAPTAIN);
            else if (player_cards[1] == CONTESSA) legal_actions.push_back(LOSE_CONTESSA);
            else if (player_cards[1] == DUKE) legal_actions.push_back(LOSE_DUKE);
        }
    }

    return legal_actions;
}

// Consideration 1: CHALLENGE outcome must be resolved.
// Correct challenge -> Previous action loses effect.
// Incorrect challenge -> Previous action stays.

// Consideration 2: Turn order correction
// If a CHALLENGE occurs, the turn order may not advance — meaning the same player could take two turns in a row.
// Therefore, LOSE_CARD ensures the turn order is swapped at the end of the turn.
// Note: If player holds the two same alive cards, LOSE_CARD loses the cards[0] first.
void GameState::lose_card(Card card) {
    std::array<Card, 2> player_cards = (current_player == 0 ? p1_cards : p2_cards);
    std::array<int, 2>& player_influence = (current_player == 0 ? p1_influence : p2_influence);
    if (player_cards[0] == card && player_influence[0] != 0) player_influence[0] = 0;
    else if (player_cards[1] == card && player_influence[1] != 0) player_influence[1] = 0;
    else assert(false && "lose_card() has no card to lose.");

    // Consideration 1: Challenge resolution
    // Correct challenge: CHALLENGE -> LOSE CARD 
    if (history[history.size() - 2] == CHALLENGE) {
        Action challenged_action = history[history.size() - 3];
        if (challenged_action == TAX) (current_player == 0 ? p1_coins : p2_coins) -= 3;
        else if (challenged_action == STEAL1 || challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL1_CAP) {
            (current_player == 0 ? p1_coins : p2_coins) -= 1;
            (current_player == 0 ? p2_coins : p1_coins) += 1;
        }
        else if (challenged_action == STEAL2 || challenged_action == BLOCK_STEAL2_AMB || challenged_action == BLOCK_STEAL2_CAP) {
            (current_player == 0 ? p1_coins : p2_coins) -= 2;
            (current_player == 0 ? p2_coins : p1_coins) += 2;
        }
        else if (challenged_action == ASSASSINATE) (current_player == 0 ? p1_coins : p2_coins) += 3;
        else if (challenged_action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_coins : p1_coins) += 2;
    }

    // Consideration 2: Turn order correction
    std::vector<Action> starting_actions = {INCOME, FOREIGN_AID, TAX, STEAL1, STEAL2, ASSASSINATE, COUP};
    for (size_t i = 1; i <= 5; i++) { // 5 is the max number of actions per turn
        // Find out how many actions were taken in the current turn
        if (std::find(starting_actions.begin(), starting_actions.end(), history[history.size() - i]) != starting_actions.end()) {
            // Odd # of actions = Correct alternation
            if (i % 2 != 0) ; 
            // Even # of actions = Incorrect alternation -> Apply correction
            else current_player = 1 - current_player;
            return;
        }
    }
}

void GameState::undo_lose_card(Card card) {
    std::array<Card, 2> player_cards = (current_player == 0 ? p1_cards : p2_cards);
    std::array<int, 2>& player_influence = (current_player == 0 ? p1_influence : p2_influence);
    if (player_cards[0] == card && player_influence[0] == 0) player_influence[0] = 1;
    else if (player_cards[1] == card && player_influence[1] == 0) player_influence[1] = 1;
    else assert(false && "Undoing lose_card() has gained no card back.");

    // Undoing challenge resolution
    if (history[history.size() - 2] == CHALLENGE) {
        Action challenged_action = history[history.size() - 3];
        if (challenged_action == TAX) (current_player == 0 ? p1_coins : p2_coins) += 3;
        else if (challenged_action == STEAL1 || challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL1_CAP) {
            (current_player == 0 ? p1_coins : p2_coins) += 1;
            (current_player == 0 ? p2_coins : p1_coins) -= 1;
        }
        else if (challenged_action == STEAL2 || challenged_action == BLOCK_STEAL2_AMB || challenged_action == BLOCK_STEAL2_CAP) {
            (current_player == 0 ? p1_coins : p2_coins) += 2;
            (current_player == 0 ? p2_coins : p1_coins) -= 2;
        }
        else if (challenged_action == ASSASSINATE) (current_player == 0 ? p1_coins : p2_coins) -= 3;
        else if (challenged_action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_coins : p1_coins) -= 2;
    }

    // Undoing turn order correction
    std::vector<Action> starting_actions = {INCOME, FOREIGN_AID, TAX, STEAL1, STEAL2, ASSASSINATE, COUP};
    for (size_t i = 1; i <= 5; i++) { // 5 is the max number of actions per turn
        // Find out how many actions were taken in the current turn
        if (std::find(starting_actions.begin(), starting_actions.end(), history[history.size() - i]) != starting_actions.end()) {
            // Even # of actions = Correct alternation
            if (i % 2 == 0) ; 
            // Odd # of actions = Incorrect alternation -> Apply correction
            else current_player = 1 - current_player;
            return;
        }
    }
}

void GameState::apply_action(Action action) {
    history.push_back(action);
    if (action == INCOME) (current_player == 0 ? p1_coins : p2_coins) += 1;
    else if (action == FOREIGN_AID) (current_player == 0 ? p1_coins : p2_coins) += 2;
    else if (action == TAX) (current_player == 0 ? p1_coins : p2_coins) += 3;
    else if (action == STEAL1 || action == BLOCK_STEAL1_AMB || action == BLOCK_STEAL1_CAP) {
        (current_player == 0 ? p1_coins : p2_coins) += 1;
        (current_player == 0 ? p2_coins : p1_coins) -= 1;
    }
    else if (action == STEAL2 || action == BLOCK_STEAL2_AMB || action == BLOCK_STEAL2_CAP) {
        (current_player == 0 ? p1_coins : p2_coins) += 2;
        (current_player == 0 ? p2_coins : p1_coins) -= 2;
    }
    else if (action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) -= COIN_TO_ASSASSINATE;
    }
    else if (action == COUP) (current_player == 0 ? p1_coins : p2_coins) -= COIN_TO_COUP;
    else if (action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_coins : p1_coins) -= 2;

    // LOSE_CARD
    else if (action == LOSE_ASSASSIN) lose_card(ASSASSIN);
    else if (action == LOSE_AMBASSADOR) lose_card(AMBASSADOR);
    else if (action == LOSE_CAPTAIN) lose_card(CAPTAIN);
    else if (action == LOSE_CONTESSA) lose_card(CONTESSA);
    else if (action == LOSE_DUKE) lose_card(DUKE);

    // LOSE_BOTH
    else if (action == LOSE_BOTH) {
        if (current_player == 0) p1_influence = {0, 0};
        else p2_influence = {0, 0};
    }
        
    // Update block counter
    if (action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_num_fa_blocked : p1_num_fa_blocked) += 1;
    else if (action == BLOCK_STEAL1_AMB || action == BLOCK_STEAL2_AMB) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) += 1;
    else if (action == BLOCK_STEAL1_CAP || action == BLOCK_STEAL2_CAP) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) += 1;
    else if (action == BLOCK_ASSASSINATE) (current_player == 0 ? p2_num_assassinate_blocked : p1_num_assassinate_blocked) += 1;

    current_player = 1 - current_player;
}

void GameState::undo_action() {
    Action last_action = history.back();
    history.pop_back();
    current_player = 1 - current_player;
    if (last_action == INCOME) (current_player == 0 ? p1_coins : p2_coins) -= 1;
    else if (last_action == FOREIGN_AID) (current_player == 0 ? p1_coins : p2_coins) -= 2;
    else if (last_action == TAX) (current_player == 0 ? p1_coins : p2_coins) -= 3;
    else if (last_action == STEAL1 || last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL1_CAP) {
        (current_player == 0 ? p1_coins : p2_coins) -= 1;
        (current_player == 0 ? p2_coins : p1_coins) += 1;
    }
    else if (last_action == STEAL2 || last_action == BLOCK_STEAL2_AMB || last_action == BLOCK_STEAL2_CAP) {
        (current_player == 0 ? p1_coins : p2_coins) -= 2;
        (current_player == 0 ? p2_coins : p1_coins) += 2;
    }
    else if (last_action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) += COIN_TO_ASSASSINATE;
    }
    else if (last_action == COUP) (current_player == 0 ? p1_coins : p2_coins) += COIN_TO_COUP;
    else if (last_action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_coins : p1_coins) += 2;
    else if (last_action == LOSE_ASSASSIN) undo_lose_card(ASSASSIN);
    else if (last_action == LOSE_AMBASSADOR) undo_lose_card(AMBASSADOR);
    else if (last_action == LOSE_CAPTAIN) undo_lose_card(CAPTAIN);
    else if (last_action == LOSE_CONTESSA) undo_lose_card(CONTESSA);
    else if (last_action == LOSE_DUKE) undo_lose_card(DUKE);

    else if (last_action == LOSE_BOTH) {
        if (current_player == 0) p1_influence = {1, 1};
        else p2_influence = {1, 1};
    }

    // Update block counter
    if (last_action == BLOCK_FOREIGN_AID) (current_player == 0 ? p2_num_fa_blocked : p1_num_fa_blocked) -= 1;
    else if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) -= 1;
    else if (last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) -= 1;
    else if (last_action == BLOCK_ASSASSINATE) (current_player == 0 ? p2_num_assassinate_blocked : p1_num_assassinate_blocked) -= 1;
}

size_t GameState::get_hash() {
    Card card1 = (current_player == 0) ? p1_cards[0] : p2_cards[0];
    Card card2 = (current_player == 0) ? p1_cards[1] : p2_cards[1];
    size_t hash = std::hash<int>{}(static_cast<int>(card1));
    hash ^= std::hash<int>{}(static_cast<int>(card2)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    for (const Action& action : history) {
        // Combine with existing hash using standard technique
        hash ^= std::hash<int>{}(static_cast<int>(action)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}

std::string GameState::get_infoset_string() const {
    std::string infoset_str = "";
    std::array<Card, 2> current_cards = (current_player == 0 ? p1_cards : p2_cards);

    for (size_t i = 0; i < 2; i++) {
        if (current_cards[i] == ASSASSIN) infoset_str += "ASSASSIN ";
        else if (current_cards[i] == AMBASSADOR) infoset_str += "AMBASSADOR ";
        else if (current_cards[i] == CAPTAIN) infoset_str += "CAPTAIN ";
        else if (current_cards[i] == CONTESSA) infoset_str += "CONTESSA ";
        else if (current_cards[i] == DUKE) infoset_str += "DUKE ";
    }
    infoset_str += ": ";
    // Add action history
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) infoset_str += ", ";
        
        Action action = history[i];
        if (action == INCOME) infoset_str += "INCOME";
        else if (action == FOREIGN_AID) infoset_str += "FOREIGN_AID";
        else if (action == TAX) infoset_str += "TAX";
        else if (action == STEAL1) infoset_str += "STEAL1";
        else if (action == STEAL2) infoset_str += "STEAL2";
        else if (action == ASSASSINATE) infoset_str += "ASSASSINATE";
        else if (action == COUP) infoset_str += "COUP";
        else if (action == BLOCK_FOREIGN_AID) infoset_str += "BLOCK_FOREIGN_AID";
        else if (action == BLOCK_STEAL1_AMB) infoset_str += "BLOCK_STEAL1_AMB";
        else if (action == BLOCK_STEAL2_AMB) infoset_str += "BLOCK_STEAL2_AMB";
        else if (action == BLOCK_STEAL1_CAP) infoset_str += "BLOCK_STEAL1_CAP";
        else if (action == BLOCK_STEAL2_CAP) infoset_str += "BLOCK_STEAL2_CAP";
        else if (action == BLOCK_ASSASSINATE) infoset_str += "BLOCK_ASSASSINATE";
        else if (action == CHALLENGE) infoset_str += "CHALLENGE";
        else if (action == PASS_BLOCK) infoset_str += "PASS_BLOCK";
        else if (action == SHOW_ASSASSIN) infoset_str += "SHOW_ASSASSIN";
        else if (action == SHOW_AMBASSADOR) infoset_str += "SHOW_AMBASSADOR";
        else if (action == SHOW_CAPTAIN) infoset_str += "SHOW_CAPTAIN";
        else if (action == SHOW_CONTESSA) infoset_str += "SHOW_CONTESSA";
        else if (action == SHOW_DUKE) infoset_str += "SHOW_DUKE";
        else if (action == LOSE_ASSASSIN) infoset_str += "LOSE_ASSASSIN";
        else if (action == LOSE_AMBASSADOR) infoset_str += "LOSE_AMBASSADOR";
        else if (action == LOSE_CAPTAIN) infoset_str += "LOSE_CAPTAIN";
        else if (action == LOSE_CONTESSA) infoset_str += "LOSE_CONTESSA";
        else if (action == LOSE_DUKE) infoset_str += "LOSE_DUKE";
        else if (action == LOSE_BOTH) infoset_str += "LOSE_BOTH";
    }
    
    return infoset_str;
}


// For debugging
void GameState::print_game_state() {
    std::cout << "Current player: " << current_player << std::endl;
    std::cout << "P1 Cards: " << p1_cards[0] << (p1_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p1_cards[1] << (p1_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;
    std::cout << "P2 Cards: " << p2_cards[0] << (p2_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p2_cards[1] << (p2_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;

    std::cout << "P1 Coins: " << p1_coins << std::endl;
    std::cout << "P2 Coins: " << p2_coins << std::endl;

    std::cout << "History: ";
    for (Action a : history) {
        std::cout << action_to_string.at(a) << " ";
    }
    std::cout << std::endl;
    std::cout << "Possible actions: ";
    for (Action a : get_legal_actions()) {
        std::cout << action_to_string.at(a) << " ";
    }
    std::cout << std::endl << std::endl;
}

// For debugging
std::string GameState::get_game_state() {
    std::string output_string = "";
    
    output_string += "Current player: " + std::to_string(current_player) + "\n";
    
    output_string += "P1 Cards: " + std::to_string(p1_cards[0]) + 
                     (p1_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") +
                     std::to_string(p1_cards[1]) + 
                     (p1_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") + "\n";
    
    output_string += "P2 Cards: " + std::to_string(p2_cards[0]) + 
                     (p2_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") +
                     std::to_string(p2_cards[1]) + 
                     (p2_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") + "\n";
    
    output_string += "P1 Coins: " + std::to_string(p1_coins) + "\n";
    output_string += "P2 Coins: " + std::to_string(p2_coins) + "\n";
    
    output_string += "History: ";
    for (Action a : history) {
        output_string += action_to_string.at(a) + " ";
    }
    output_string += "\n";
    
    output_string += "Possible actions: ";
    for (Action a : get_legal_actions()) {
        output_string += action_to_string.at(a) + " ";
    }
    output_string += "\n\n";
    
    return output_string;
}