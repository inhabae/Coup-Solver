#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

static const char* const CARD_NAMES[] = {
    "ASSASSIN", "AMBASSADOR", "CAPTAIN", "CONTESSA", "DUKE"
};

static const char* const ACTION_NAMES[] = {
    "INCOME", "FOREIGN_AID", "TAX", "STEAL1", "STEAL2", "ASSASSINATE", "COUP",
    "BLOCK_FOREIGN_AID", "BLOCK_STEAL1_AMB", "BLOCK_STEAL2_AMB", 
    "BLOCK_STEAL1_CAP", "BLOCK_STEAL2_CAP", "BLOCK_ASSASSINATE",
    "CHALLENGE", "PASS_BLOCK",
    "SHOW_ASSASSIN", "SHOW_AMBASSADOR", "SHOW_CAPTAIN", "SHOW_CONTESSA", "SHOW_DUKE",
    "LOSE_ASSASSIN", "LOSE_AMBASSADOR", "LOSE_CAPTAIN", "LOSE_CONTESSA", "LOSE_DUKE",
    "LOSE_BOTH"
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
    const bool p1_dead = (p1_influence[0] == 0 && p1_influence[1] == 0);
    const bool p2_dead = (p2_influence[0] == 0 && p2_influence[1] == 0);
    return p1_dead || p2_dead;
}

void GameState::set_cards(Card p1_card1, Card p1_card2, Card p2_card1, Card p2_card2) {
    auto [p1_min, p1_max] = std::minmax(p1_card1, p1_card2);
    auto [p2_min, p2_max] = std::minmax(p2_card1, p2_card2);
    p1_cards = {p1_min, p1_max};
    p2_cards = {p2_min, p2_max};
}

void GameState::set_my_cards(const std::array<Card, 2> cards) {
    if (current_player == 0) p1_cards = cards;
    else p2_cards = cards;
}

double GameState::get_utility() const {
    const bool p1_dead = (p1_influence[0] == 0 && p1_influence[1] == 0);
    const bool p2_dead = (p2_influence[0] == 0 && p2_influence[1] == 0);

    if (current_player == 0) {
        if (p1_dead) return -1.0;
        if (p2_dead) return 1.0;
    } else {
        if (p1_dead) return 1.0;
        if (p2_dead) return -1.0;
    }
    
    assert(false && "Invalid state in get_utility(): game not terminal");
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
    if (is_terminal()) return {};
    if (history.empty()) return {INCOME, FOREIGN_AID, TAX, STEAL2};

    const Action last_action = history.back();
    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const int opp_coins = is_p1 ? p2_coins : p1_coins;
    const std::array<Card, 2>& my_cards = is_p1 ? p1_cards : p2_cards;
    const std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    const std::array<int, 2>& opp_influence = is_p1 ? p2_influence : p1_influence;
    const int my_lives = my_influence[0] + my_influence[1];
    const int opp_lives = opp_influence[0] + opp_influence[1];
    
    // Helper lambda for forced/priority coups
    auto must_coup = [&]() -> bool {
        return my_coins >= COIN_TO_MUST_COUP || 
               (my_coins >= COIN_TO_COUP && opp_lives == 1);
    };
    
    // Helper lambda for base actions
    auto get_base_actions = [&]() -> std::vector<Action> {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {INCOME, FOREIGN_AID, TAX};
        if (opp_coins == 1) actions.push_back(STEAL1);
        else if (opp_coins >= 2) actions.push_back(STEAL2);
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        
        return actions;
    };
    
    // vs Action that leads to a new turn
    if (last_action == INCOME || last_action == PASS_BLOCK || 
        (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE)) {
        return get_base_actions();
    }
    
    // vs Foreign Aid
    if (last_action == FOREIGN_AID) {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {INCOME, FOREIGN_AID, TAX, STEAL2};
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        actions.push_back(BLOCK_FOREIGN_AID);
        return actions;
    }
    
    // vs Tax
    if (last_action == TAX) {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {INCOME, FOREIGN_AID, TAX, STEAL2};
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        actions.push_back(CHALLENGE);
        return actions;
    }
    
    // vs Steal 1
    if (last_action == STEAL1) {
        return {TAX, BLOCK_STEAL1_AMB, BLOCK_STEAL1_CAP, CHALLENGE};
    }
    
    // vs Steal 2
    if (last_action == STEAL2) {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {TAX};
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        actions.insert(actions.end(), {BLOCK_STEAL2_AMB, BLOCK_STEAL2_CAP, CHALLENGE});
        return actions;
    }
    
    // vs Assassinate
    if (last_action == ASSASSINATE) {
        std::vector<Action> actions = {BLOCK_ASSASSINATE, CHALLENGE};
        
        // If only one life, must block or challenge (no voluntary loss)
        if (my_lives == 1) return actions;
        std::vector<Action> lose_actions = get_card_losing_actions(my_cards, my_influence);
        actions.insert(actions.end(), lose_actions.begin(), lose_actions.end());
        return actions;
    }
    
    // Block responses - challenge or pass
    if (last_action == BLOCK_FOREIGN_AID) {
        const bool was_blocked = is_p1 ? p1_num_fa_blocked >= MAX_BLOCK_NUM : p2_num_fa_blocked >= MAX_BLOCK_NUM;
        return was_blocked ? std::vector<Action>{CHALLENGE} : std::vector<Action>{CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB || 
        last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) {
        const bool was_blocked = is_p1 ? p1_num_steal_blocked >= MAX_BLOCK_NUM : p2_num_steal_blocked >= MAX_BLOCK_NUM;
        return was_blocked ? std::vector<Action>{CHALLENGE} : std::vector<Action>{CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_ASSASSINATE) {
        const bool was_blocked = is_p1 ? p1_num_assassinate_blocked >= MAX_BLOCK_NUM : p2_num_assassinate_blocked >= MAX_BLOCK_NUM;
        return was_blocked ? std::vector<Action>{CHALLENGE} : std::vector<Action>{CHALLENGE, PASS_BLOCK};
    }
    
    // vs Coup
    if (last_action == COUP) {
        return get_card_losing_actions(my_cards, my_influence);
    }
    
    // vs Challenge
    if (last_action == CHALLENGE) {
        const Action challenged_action = history[history.size() - 2];
        
        // Check if we can show a valid card
        for (int i = 0; i < 2; i++) {
            if (my_influence[i] == 0) continue;
            
            const Card card = my_cards[i];
            if (card == DUKE && (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID)) {
                return {SHOW_DUKE};
            }
            if (card == ASSASSIN && challenged_action == ASSASSINATE) {
                return {SHOW_ASSASSIN};
            }
            if (card == CONTESSA && challenged_action == BLOCK_ASSASSINATE) {
                return {SHOW_CONTESSA};
            }
            if (card == CAPTAIN && (challenged_action == STEAL1 || challenged_action == STEAL2 || 
                                    challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP)) {
                return {SHOW_CAPTAIN};
            }
            if (card == AMBASSADOR && (challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL2_AMB)) {
                return {SHOW_AMBASSADOR};
            }
        }
        
        // Check for double assassination special case
        if (my_lives == 2 && challenged_action == BLOCK_ASSASSINATE) {
            return {LOSE_BOTH};
        }
        
        return get_card_losing_actions(my_cards, my_influence);
    }
    
    // vs Show card
    if (last_action >= SHOW_ASSASSIN && last_action <= SHOW_DUKE) {
        // Check for double assassination special case
        if (my_lives == 2 && last_action == SHOW_ASSASSIN) {
            return {LOSE_BOTH};
        }
        return get_card_losing_actions(my_cards, my_influence);
    }
    
    assert(false && "Invalid state in get_legal_actions()");
}

std::vector<Action> GameState::get_card_losing_actions(const std::array <Card, 2> player_cards, const std::array<int, 2> player_influence) const {
    std::vector<Action> legal_actions = {};
    legal_actions.reserve(2);

    // Helper lambda to convert Card to corresponding LOSE action
    auto card_to_lose_action = [](Card card) -> Action {
        switch (card) {
            case ASSASSIN:   return LOSE_ASSASSIN;
            case AMBASSADOR: return LOSE_AMBASSADOR;
            case CAPTAIN:    return LOSE_CAPTAIN;
            case CONTESSA:   return LOSE_CONTESSA;
            case DUKE:       return LOSE_DUKE;
            default:         assert(false && "Invalid card type");
        }
        return LOSE_ASSASSIN; // Unreachable
    };
    
    // Check card 1
    if (player_influence[0] == 1) {
        legal_actions.push_back(card_to_lose_action(player_cards[0]));
    }
    
    // Check card 2
    if (player_influence[1] == 1) {
        // Only add if it's a different card OR same card but card 1 is dead
        const bool different_cards = (player_cards[0] != player_cards[1]);
        const bool same_card_but_first_dead = (player_cards[0] == player_cards[1] && 
                                                player_influence[0] == 0);
        
        if (different_cards || same_card_but_first_dead) {
            legal_actions.push_back(card_to_lose_action(player_cards[1]));
        }
    }
    
    return legal_actions;
}

void GameState::lose_card(Card card) {
    const bool is_p1 = (current_player == 0);
    std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    const std::array<Card, 2>& my_cards = is_p1 ? p1_cards : p2_cards;
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
    
    // Lose the card
    if (my_cards[0] == card && my_influence[0] != 0) {
        my_influence[0] = 0;
    } else if (my_cards[1] == card && my_influence[1] != 0) {
        my_influence[1] = 0;
    } else {
        assert(false && "lose_card() has no card to lose.");
    }

    // Challenge resolution
    const size_t hist_size = history.size();
    if (hist_size >= 2 && history[hist_size - 2] == CHALLENGE) {
        const Action challenged_action = history[hist_size - 3];
        switch (challenged_action) {
            case TAX:
                my_coins -= 3;
                break;
            case STEAL1:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:
                my_coins -= 1;
                opp_coins += 1;
                break;
            case STEAL2:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:
                my_coins -= 2;
                opp_coins += 2;
                break;
            case ASSASSINATE:
                my_coins += 3;
                break;
            case BLOCK_FOREIGN_AID:
                opp_coins += 2;
                break;
            default:
                break;
        }
    }

    // Turn order correction
    constexpr size_t MAX_ACTIONS_PER_TURN = 5;
    for (size_t i = 1; i <= MAX_ACTIONS_PER_TURN; i++) {
        const Action action = history[hist_size - i];
        
        // Check if it's a starting action (0-6)
        if (action <= COUP) {
            // Even # of actions means incorrect alternation
            if (i % 2 == 0) {
                current_player = 1 - current_player;
            }
            return;
        }
    }
}

void GameState::undo_lose_card(Card card) {
    // Turn order correction comes FIRST
    const size_t hist_size = history.size();
    constexpr size_t MAX_ACTIONS_PER_TURN = 5;
    
    for (size_t i = 1; i <= MAX_ACTIONS_PER_TURN; i++) {
        const Action action = history[hist_size - i];
        
        // Check if it's a starting action (0-6)
        if (action <= COUP) {
            // Odd # of actions means incorrect alternation (inverted from lose_card)
            if (i % 2 != 0) {
                current_player = 1 - current_player;
            }
            break;
        }
    }

    const bool is_p1 = (current_player == 0);
    std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    const std::array<Card, 2>& my_cards = is_p1 ? p1_cards : p2_cards;
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
    
    // Restore the card
    if (my_cards[0] == card && my_influence[0] == 0) {
        my_influence[0] = 1;
    } else if (my_cards[1] == card && my_influence[1] == 0) {
        my_influence[1] = 1;
    } else {
        assert(false && "Undoing lose_card() has gained no card back.");
    }

    // Undo challenge resolution
    if (hist_size >= 1 && history[hist_size - 1] == CHALLENGE) {
        const Action challenged_action = history[hist_size - 2];
        switch (challenged_action) {
            case TAX:
                my_coins += 3;
                break;
            case STEAL1:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:
                my_coins += 1;
                opp_coins -= 1;
                break;
            case STEAL2:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:
                my_coins += 2;
                opp_coins -= 2;
                break;
            case ASSASSINATE:
                my_coins -= 3;
                break;
            case BLOCK_FOREIGN_AID:
                opp_coins -= 2;
                break;
            default:
                break;
        }
    }
}

void GameState::apply_action(Action action) {
    history.push_back(action);
    
    const bool is_p1 = (current_player == 0);
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
    int& opp_fa_blocked = is_p1 ? p2_num_fa_blocked : p1_num_fa_blocked;
    int& opp_steal_blocked = is_p1 ? p2_num_steal_blocked : p1_num_steal_blocked;
    int& opp_assassinate_blocked = is_p1 ? p2_num_assassinate_blocked : p1_num_assassinate_blocked;
    std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    
    switch (action) {
        case INCOME:
            my_coins += 1;
            break;
            
        case FOREIGN_AID:
            my_coins += 2;
            break;
            
        case TAX:
            my_coins += 3;
            break;
            
        case STEAL1:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP:
            my_coins += 1;
            opp_coins -= 1;
            break;
            
        case STEAL2:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP:
            my_coins += 2;
            opp_coins -= 2;
            break;
            
        case ASSASSINATE:
            my_coins -= COIN_TO_ASSASSINATE;
            break;
            
        case COUP:
            my_coins -= COIN_TO_COUP;
            break;
            
        case BLOCK_FOREIGN_AID:
            opp_coins -= 2;
            opp_fa_blocked += 1;
            break;
            
        case BLOCK_ASSASSINATE:
            opp_assassinate_blocked += 1;
            break;
            
        case LOSE_ASSASSIN:
            lose_card(ASSASSIN);
            break;
            
        case LOSE_AMBASSADOR:
            lose_card(AMBASSADOR);
            break;
            
        case LOSE_CAPTAIN:
            lose_card(CAPTAIN);
            break;
            
        case LOSE_CONTESSA:
            lose_card(CONTESSA);
            break;
            
        case LOSE_DUKE:
            lose_card(DUKE);
            break;
            
        case LOSE_BOTH:
            my_influence = {0, 0};
            break;
            
        default:
            // Actions that don't modify state: CHALLENGE, PASS_BLOCK, SHOW_*
            break;
    }
    
    // Update block counters for steal blocks
    if (action == BLOCK_STEAL1_AMB || action == BLOCK_STEAL2_AMB ||
        action == BLOCK_STEAL1_CAP || action == BLOCK_STEAL2_CAP) {
        opp_steal_blocked += 1;
    }
    
    current_player = 1 - current_player;
}

void GameState::undo_action() {
    const Action last_action = history.back();
    history.pop_back();
    current_player = 1 - current_player;
    
    const bool is_p1 = (current_player == 0);
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
    int& opp_fa_blocked = is_p1 ? p2_num_fa_blocked : p1_num_fa_blocked;
    int& opp_steal_blocked = is_p1 ? p2_num_steal_blocked : p1_num_steal_blocked;
    int& opp_assassinate_blocked = is_p1 ? p2_num_assassinate_blocked : p1_num_assassinate_blocked;
    std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    
    switch (last_action) {
        case INCOME:
            my_coins -= 1;
            break;
            
        case FOREIGN_AID:
            my_coins -= 2;
            break;
            
        case TAX:
            my_coins -= 3;
            break;
            
        case STEAL1:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP:
            my_coins -= 1;
            opp_coins += 1;
            break;
            
        case STEAL2:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP:
            my_coins -= 2;
            opp_coins += 2;
            break;
            
        case ASSASSINATE:
            my_coins += COIN_TO_ASSASSINATE;
            break;
            
        case COUP:
            my_coins += COIN_TO_COUP;
            break;
            
        case BLOCK_FOREIGN_AID:
            opp_coins += 2;
            opp_fa_blocked -= 1;
            break;
            
        case BLOCK_ASSASSINATE:
            opp_assassinate_blocked -= 1;
            break;
            
        case LOSE_ASSASSIN:
            undo_lose_card(ASSASSIN);
            break;
            
        case LOSE_AMBASSADOR:
            undo_lose_card(AMBASSADOR);
            break;
            
        case LOSE_CAPTAIN:
            undo_lose_card(CAPTAIN);
            break;
            
        case LOSE_CONTESSA:
            undo_lose_card(CONTESSA);
            break;
            
        case LOSE_DUKE:
            undo_lose_card(DUKE);
            break;
            
        case LOSE_BOTH:
            my_influence = {1, 1};
            break;
            
        default:
            // Actions that don't modify state: CHALLENGE, PASS_BLOCK, SHOW_*
            break;
    }
    
    // Update block counters for steal blocks
    if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB ||
        last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) {
        opp_steal_blocked -= 1;
    }
}

size_t GameState::get_hash() const {
    const std::array<Card, 2>& my_cards = (current_player == 0) ? p1_cards : p2_cards;
    
    // FNV-1a hash algorithm - fast and good distribution
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    constexpr size_t FNV_PRIME = 1099511628211ULL;
    
    hash ^= static_cast<size_t>(my_cards[0]);
    hash *= FNV_PRIME;
    hash ^= static_cast<size_t>(my_cards[1]);
    hash *= FNV_PRIME;
    
    for (const Action action : history) {
        hash ^= static_cast<size_t>(action);
        hash *= FNV_PRIME;
    }
    
    return hash;
}

std::string GameState::get_infoset_string() const {
    const std::array<Card, 2>& my_cards = (current_player == 0) ? p1_cards : p2_cards;
    
    // Pre-calculate approximate size to avoid reallocations
    // Cards: ~20 chars, history: ~15 chars per action
    std::string infoset_str;
    infoset_str.reserve(50 + history.size() * 15);
    
    infoset_str += CARD_NAMES[my_cards[0]];
    infoset_str += ' ';
    infoset_str += CARD_NAMES[my_cards[1]];
    infoset_str += ": ";
    
    if (!history.empty()) {
        infoset_str += ACTION_NAMES[history[0]];
        for (size_t i = 1; i < history.size(); ++i) {
            infoset_str += ", ";
            infoset_str += ACTION_NAMES[history[i]];
        }
    }
    
    return infoset_str;
}


// For debugging
void GameState::print_game_state() const {
    std::cout << "Current player: " << current_player << std::endl;
    std::cout << "P1 Cards: " << p1_cards[0] << (p1_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p1_cards[1] << (p1_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;
    std::cout << "P2 Cards: " << p2_cards[0] << (p2_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p2_cards[1] << (p2_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;

    std::cout << "P1 Coins: " << p1_coins << std::endl;
    std::cout << "P2 Coins: " << p2_coins << std::endl;

    std::cout << "History: ";
    for (Action a : history) {
        std::cout << ACTION_NAMES[a] << " ";
    }
    std::cout << std::endl;
    std::cout << "Possible actions: ";
    for (Action a : get_legal_actions()) {
        std::cout << ACTION_NAMES[a] << " ";
    }
    std::cout << std::endl << std::endl;
}

// For debugging
std::string GameState::get_game_state() const {
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
        output_string += ACTION_NAMES[a];
        output_string += " ";
    }
    output_string += "\n";
    
    output_string += "Possible actions: ";
    for (Action a : get_legal_actions()) {
        output_string += ACTION_NAMES[a];
        output_string += " ";
    }

    output_string += "\nBlock count variables: ";
    output_string += std::to_string(p1_num_assassinate_blocked) + " ";
    output_string += std::to_string(p2_num_assassinate_blocked) + " ";
    output_string += std::to_string(p1_num_steal_blocked) + " ";
    output_string += std::to_string(p2_num_steal_blocked) + " ";
    output_string += std::to_string(p1_num_fa_blocked) + " ";
    output_string += std::to_string(p2_num_fa_blocked);

    output_string += "\n\n";
    
    return output_string;
}