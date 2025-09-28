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
        return has_correct_card ? 1.0 : -1.0;
    }
    assert(false && "get_utility() called on non-terminal state");
}

int GameState::get_current_player() const {
    return current_player;
}

std::vector<Action> GameState::get_legal_actions() const {
    // Game start
    if (history.size() == 0) return {INCOME, TAX};

    Action last_action = history.back();
    int current_player_coins = (current_player == 0) ? p1_coins : p2_coins;
    int current_player_assassinated = (current_player == 0) ? p1_did_assassinate : p2_did_assassinate;
    // vs Income, Pass_block
    if (last_action == INCOME || last_action == PASS_BLOCK) {
        if (current_player_coins >= 3) return {COUP};
        if (current_player_coins == 2 && !current_player_assassinated) {
            return {INCOME, TAX, ASSASSINATE};
        }
        return {INCOME, TAX};
    }
    // vs Tax
    else if (last_action == TAX) {
        if (current_player_coins >= 3) return {COUP};
        if (current_player_coins == 2 && !current_player_assassinated) {
            return {INCOME, TAX, ASSASSINATE, CHALLENGE};
        }
        return {INCOME, TAX, CHALLENGE};
    }
    // vs Assassinate
    else if (last_action == ASSASSINATE) {
        return {BLOCK_ASSASSINATE, CHALLENGE};
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
    else if (action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) -= 2;
        (current_player == 0 ? p1_did_assassinate : p2_did_assassinate) = true;
    }
    else if (action == COUP) (current_player == 0 ? p1_coins : p2_coins) -= 3;
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
    else if (last_action == ASSASSINATE) {
        (current_player == 0 ? p1_coins : p2_coins) += 2;
        (current_player == 0 ? p1_did_assassinate : p2_did_assassinate) = false;
    }
    else if (last_action == COUP) {
        (current_player == 0 ? p1_coins : p2_coins) += 3;
    }
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
    else if (current_card == DUKE) infoset_str += "DUKE: ";
    else if (current_card == CONTESSA) infoset_str += "CONTESSA: ";
    
    // Add action history
    for (int i = 0; i < history.size(); i++) {
        if (i > 0) infoset_str += ", ";
        
        Action action = history[i];
        if (action == INCOME) infoset_str += "INCOME";
        else if (action == TAX) infoset_str += "TAX";
        else if (action == ASSASSINATE) infoset_str += "ASSASSINATE";
        else if (action == COUP) infoset_str += "COUP";
        else if (action == BLOCK_ASSASSINATE) infoset_str += "BLOCK_ASSASSINATE";
        else if (action == CHALLENGE) infoset_str += "CHALLENGE";
        else if (action == PASS_BLOCK) infoset_str += "PASS_BLOCK";
    }
    
    return infoset_str;
}