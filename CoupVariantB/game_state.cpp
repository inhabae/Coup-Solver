#include "game_state.hpp"

#include <cassert>

GameState::GameState() {
    current_player = 0;
    p1_card = ASSASSIN;
    p2_card = ASSASSIN;
    p1_coins = 0;
    p2_coins = 0;
    p1_did_assassinate = false;
    p2_did_assassinate = false;
    p1_num_stolen = 0;
    p2_num_stolen = 0;
    p1_num_steal_blocked = 0;
    p2_num_steal_blocked = 0;
    history = {};
}
bool GameState::is_terminal() const {
    if (history.size() == 0) return false;
    return history.back() == COUP || history.back() == CHALLENGE;
}

void GameState::set_cards(Card card1, Card card2) {
    p1_card = card1;
    p2_card = card2;
}

// For best response function
void GameState::set_my_card(Card card) {
    if (current_player == 0) p1_card = card;
    else p2_card = card;
}

double GameState::get_utility() const {
    Action last_action = history.back();
    // Coup
    if (last_action == COUP) {
        return -1.0;
    }
    // Challenge
    else if (last_action == CHALLENGE) {
        Action challenged_action = history[history.size() - 2];
        Card current_card = (current_player == 0) ? p1_card : p2_card;
        bool has_correct_card = false;
        if (challenged_action == TAX && current_card == DUKE) {
            has_correct_card = true;
        }
        else if (challenged_action == ASSASSINATE && current_card == ASSASSIN) {
            has_correct_card = true;
        }
        else if (challenged_action == BLOCK_ASSASSINATE && current_card == CONTESSA) {
            has_correct_card = true;
        }
        else if (current_card == CAPTAIN) {
            if (challenged_action == STEAL1 || challenged_action == STEAL2 || challenged_action == BLOCK_STEAL1 || challenged_action == BLOCK_STEAL2) {
                has_correct_card = true;
            }
        }
        return has_correct_card ? 1.0 : -1.0;
    }
    assert(false && "get_utility() called on non-terminal state");
}
   
double GameState::get_br_utility(int maximizing_player, std::vector<double> card_distribution) const {
    Action last_action = history.back();
    // Coup
    if (last_action == COUP) return -1.0;
    // Maximizing Player is challenged -> Check card 
    if (maximizing_player == current_player) {
        Action challenged_action = history[history.size() - 2];
        Card current_card = (current_player == 0) ? p1_card : p2_card;
        bool has_correct_card = false;
        if (challenged_action == TAX && current_card == DUKE) {
            has_correct_card = true;
        }
        else if (challenged_action == ASSASSINATE && current_card == ASSASSIN) {
            has_correct_card = true;
        }
        else if (challenged_action == BLOCK_ASSASSINATE && current_card == CONTESSA) {
            has_correct_card = true;
        }
        else if (current_card == CAPTAIN) {
            if (challenged_action == STEAL1 || challenged_action == STEAL2 || challenged_action == BLOCK_STEAL1 || challenged_action == BLOCK_STEAL2) {
                has_correct_card = true;
            }
        }
        return has_correct_card ? 1.0 : -1.0;
    }
    // Non-Maximizing Player is challenged -> Consider distribution
    else {
        // Find challenged action
        Action challenged_action = history[history.size() - 2];

        Card challenged_card = ASSASSIN;
        if (challenged_action == BLOCK_ASSASSINATE) challenged_card = CONTESSA;
        else if (challenged_action == TAX) challenged_card = DUKE;
        else if (challenged_action == STEAL1 || challenged_action == STEAL2) challenged_card = CAPTAIN;
        else if (challenged_action == BLOCK_STEAL1 || challenged_action == BLOCK_STEAL2) challenged_card = CAPTAIN;
        
        std::vector<Card> cards {ASSASSIN, CAPTAIN, CONTESSA, DUKE};

        // Calculate utility based on the distribution
        double utility = 0.0;
        for (size_t c = 0; c < card_distribution.size(); c++)
            if (challenged_card == cards[c]) {
                utility += card_distribution[c] * 1.0;
            }
            else {
                utility -= card_distribution[c] * 1.0;
            } 
        return utility;
    }
    assert(false && "get_br_utility() called on non-terminal state");
}

int GameState::get_current_player() const {
    return current_player;
}

std::vector<Action> GameState::get_legal_actions() const {
    // Game start
    if (history.size() == 0) return {INCOME, TAX};

    Action last_action = history.back();
    std::vector<Action> legal_actions = {INCOME, TAX};
    int current_player_coins = (current_player == 0) ? p1_coins : p2_coins;
    int opp_coins = (current_player == 0) ? p2_coins : p1_coins;
    bool current_player_assassinated = (current_player == 0) ? p1_did_assassinate : p2_did_assassinate;
    bool current_player_was_stolen = (current_player == 0) ? p1_num_stolen >= 1 : p2_num_stolen >= 1;
    bool current_player_was_steal_blocked = (current_player == 0) ? p1_num_steal_blocked >= 1 : p2_num_steal_blocked >= 1;
    
    // vs Income, Pass_block
    if (last_action == INCOME || last_action == PASS_BLOCK) {
        if (current_player_coins >= 3) return {COUP};
        if (opp_coins == 1) legal_actions.push_back(STEAL1);
        else if (opp_coins >= 2) legal_actions.push_back(STEAL2);
        if (current_player_coins == 2 && !current_player_assassinated) legal_actions.push_back(ASSASSINATE);
        return legal_actions;
    }
    // vs Tax
    else if (last_action == TAX) {
        if (current_player_coins >= 3) return {COUP};
        if (opp_coins == 1) legal_actions.push_back(STEAL1);
        else if (opp_coins >= 2) legal_actions.push_back(STEAL2);
        if (current_player_coins == 2 && !current_player_assassinated) legal_actions.push_back(ASSASSINATE);
        legal_actions.push_back(CHALLENGE);
        return legal_actions;
    }
    // vs Steal 1
    else if (last_action == STEAL1) {
        // if (opp_coins == 1) legal_actions.push_back(STEAL1);
        // else if (opp_coins >= 2) legal_actions.push_back(STEAL2);
        if (current_player_was_stolen) legal_actions.clear();
        legal_actions.push_back(BLOCK_STEAL1);
        legal_actions.push_back(CHALLENGE);
        return legal_actions;
    }
    // vs Steal 2
    else if (last_action == STEAL2) {
        if (current_player_coins >= 3) return {COUP};
        // if (opp_coins == 1) legal_actions.push_back(STEAL1);
        // else if (opp_coins >= 2) legal_actions.push_back(STEAL2);
        if (current_player_was_stolen) legal_actions.clear();
        if (current_player_coins == 2 && !current_player_assassinated) legal_actions.push_back(ASSASSINATE);
        legal_actions.push_back(BLOCK_STEAL2);
        legal_actions.push_back(CHALLENGE);
        return legal_actions;
    }
    // vs Assassinate
    else if (last_action == ASSASSINATE) {
        return {BLOCK_ASSASSINATE, CHALLENGE};
    }
    // vs Block_steal
    else if (last_action == BLOCK_STEAL1 || last_action == BLOCK_STEAL2) {
        if (current_player_was_steal_blocked) {
            return {CHALLENGE};
        }
        return {CHALLENGE, PASS_BLOCK};
    }
    // vs Block_assassinate
    else if (last_action == BLOCK_ASSASSINATE) {
        return {CHALLENGE, PASS_BLOCK};
    }
    // vs Coup, Challenge
    else if (last_action == COUP || last_action == CHALLENGE) {
        return {};
    }
    assert(false && "Invalid state in get_legal_actions()");
}

void GameState::apply_action(Action action) {
    history.push_back(action);
    if (action == INCOME) (current_player == 0 ? p1_coins : p2_coins) += 1;
    else if (action == TAX) (current_player == 0 ? p1_coins : p2_coins) += 2;
    else if (action == STEAL1 || action == BLOCK_STEAL1) {
        (current_player == 0 ? p1_coins : p2_coins) += 1;
        (current_player == 0 ? p2_coins : p1_coins) -= 1;
    }
    else if (action == STEAL2 || action == BLOCK_STEAL2) {
        (current_player == 0 ? p1_coins : p2_coins) += 2;
        (current_player == 0 ? p2_coins : p1_coins) -= 2;
    }
    else if (action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) -= 2;
        (current_player == 0 ? p1_did_assassinate : p2_did_assassinate) = true;
    }
    else if (action == COUP) (current_player == 0 ? p1_coins : p2_coins) -= 3;
    if (action == STEAL1 || action == STEAL2) (current_player == 0 ? p2_num_stolen : p1_num_stolen) += 1;
    if (action == BLOCK_STEAL1 || action == BLOCK_STEAL2) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) += 1;

    current_player = 1 - current_player;
}

void GameState::undo_action() {
    Action last_action = history.back();
    history.pop_back();
    current_player = 1 - current_player;
    if (last_action == INCOME) {
        (current_player == 0 ? p1_coins : p2_coins) -= 1;
    }
    else if (last_action == TAX) {
        (current_player == 0 ? p1_coins : p2_coins) -= 2;
    }
    else if (last_action == STEAL1 || last_action == BLOCK_STEAL1) {
        (current_player == 0 ? p1_coins : p2_coins) -= 1;
        (current_player == 0 ? p2_coins : p1_coins) += 1;
    }
    else if (last_action == STEAL2 || last_action == BLOCK_STEAL2) {
        (current_player == 0 ? p1_coins : p2_coins) -= 2;
        (current_player == 0 ? p2_coins : p1_coins) += 2;
    }
    else if (last_action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) += 2;
        (current_player == 0 ? p1_did_assassinate : p2_did_assassinate) = false;
    }
    else if (last_action == COUP) (current_player == 0 ? p1_coins : p2_coins) += 3;
    if (last_action == STEAL1 || last_action == STEAL2) (current_player == 0 ? p2_num_stolen : p1_num_stolen) -= 1;
    if (last_action == BLOCK_STEAL1 || last_action == BLOCK_STEAL2) (current_player == 0 ? p2_num_steal_blocked : p1_num_steal_blocked) -= 1;
}

size_t GameState::get_hash() {
    Card current_card = (current_player == 0) ? p1_card : p2_card;
    size_t hash = std::hash<int>{}(static_cast<int>(current_card));
    for (const Action& action : history) {
        // Combine with existing hash using standard technique
        hash ^= std::hash<int>{}(static_cast<int>(action)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}

std::string GameState::get_infoset_string() const {
    std::string infoset_str = "";
    Card current_card = (current_player == 0) ? p1_card : p2_card;
    if (current_card == ASSASSIN) infoset_str += "ASSASSIN: ";
    else if (current_card == CAPTAIN) infoset_str += "CAPTAIN: ";
    else if (current_card == CONTESSA) infoset_str += "CONTESSA: ";
    else if (current_card == DUKE) infoset_str += "DUKE: ";
    
    // Add action history
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) infoset_str += ", ";
        
        Action action = history[i];
        if (action == INCOME) infoset_str += "INCOME";
        else if (action == TAX) infoset_str += "TAX";
        else if (action == STEAL1) infoset_str += "STEAL1";
        else if (action == STEAL2) infoset_str += "STEAL2";
        else if (action == ASSASSINATE) infoset_str += "ASSASSINATE";
        else if (action == COUP) infoset_str += "COUP";
        else if (action == BLOCK_STEAL1) infoset_str += "BLOCK_STEAL1";
        else if (action == BLOCK_STEAL2) infoset_str += "BLOCK_STEAL2";
        else if (action == BLOCK_ASSASSINATE) infoset_str += "BLOCK_ASSASSINATE";
        else if (action == CHALLENGE) infoset_str += "CHALLENGE";
        else if (action == PASS_BLOCK) infoset_str += "PASS_BLOCK";
    }
    
    return infoset_str;
}