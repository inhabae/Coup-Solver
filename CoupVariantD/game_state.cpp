#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

static const char* const CARD_NAMES[] = {
    "ASSASSIN", "AMBASSADOR", "CAPTAIN", "CONTESSA", "DUKE"
};

GameState::GameState() {
    current_player = 0;
    p1_cards = {ASSASSIN, ASSASSIN};
    p2_cards = {ASSASSIN, ASSASSIN};
    p1_influence = {1,1};
    p2_influence = {1,1};
    p1_coins = 2;
    p2_coins = 2;
    history = {};

    // MOST LIKELY #6
    num_p1_has_allowed_tax = 0;
    num_p2_has_allowed_tax = 0;

    // MOST LIKELY #6 + LIKELY #1
    num_p1_has_allowed_foreign_aid = 0;
    num_p2_has_allowed_foreign_aid = 0;
    num_p1_has_allowed_steal = 0;
    num_p2_has_allowed_steal = 0;
    num_p1_has_allowed_assassinate = 0;
    num_p2_has_allowed_assassinate = 0;

    // LIKELY #2
    num_p1_has_claimed_duke = 0;
    num_p2_has_claimed_duke = 0;
    num_p1_has_claimed_steal_blocker = 0;
    num_p2_has_claimed_steal_blocker = 0;
    num_p1_has_claimed_contessa = 0;
    num_p2_has_claimed_contessa = 0;
}

bool GameState::is_terminal() const {
    const bool p1_dead = (p1_influence[0] == 0 && p1_influence[1] == 0);
    const bool p2_dead = (p2_influence[0] == 0 && p2_influence[1] == 0);
    return p1_dead || p2_dead;
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
   
double GameState::get_br_utility(int maximizing_player, std::array<double, NUM_HOLDINGS> cards_distribution) const {
    const Action prev_action = history[history.size() - 2];
    // Coup
    if (prev_action == COUP) return 1.0;
    // Challenge
    const Action challenged_action = history[history.size() - (history[history.size() - 2] == CHALLENGE ? 3 : 4)];
    Card challenged_card;

    switch (challenged_action) {
        case BLOCK_ASSASSINATE:
            challenged_card = CONTESSA;
            break;
        case TAX:
        case BLOCK_FOREIGN_AID:
            challenged_card = DUKE;
            break;
        case STEAL1:
        case STEAL2:
        case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_CAP:
            challenged_card = CAPTAIN;
            break;
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL2_AMB:
            challenged_card = AMBASSADOR;
            break;
        default:
            assert(false && "Invalid challenged action in get_br_utility()");
            break;
    }
    // Maximizing Player is challenged -> Check card 
    if (maximizing_player == current_player) {
        const std::array<Card, 2>& current_cards = (current_player == 0) ? p1_cards : p2_cards;
        const std::array<int, 2>& current_influence = (current_player == 0) ? p1_influence : p2_influence;
        if ((current_influence[0] && current_cards[0] == challenged_card) ||
            (current_influence[1] && current_cards[1] == challenged_card)) {
            return 1.0;
        }
        return -1.0;
    }
    // Non-Maximizing Player is challenged -> Consider distribution
    else {
        double utility = 0.0;
        for (size_t h = 0; h < NUM_HOLDINGS; h++) {
            const bool has_challenged_card = (holdings[h][0] == challenged_card || 
                                            holdings[h][1] == challenged_card);
            utility += cards_distribution[h] * (has_challenged_card ? 1.0 : -1.0);
        }
        return utility;
    }
    assert(false && "get_br_utility() called on non-terminal state");
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
    const std::array<Card, 2> my_cards = is_p1 ? p1_cards : p2_cards;
    const std::array<int, 2> my_influence = is_p1 ? p1_influence : p2_influence;
    const std::array<int, 2> opp_influence = is_p1 ? p2_influence : p1_influence;
    const int my_lives = my_influence[0] + my_influence[1];
    const int opp_lives = opp_influence[0] + opp_influence[1];

    // RULE: DEFINITELY #1
    auto must_coup = [&]() -> bool {
        return my_coins >= COIN_TO_MUST_COUP || 
               (my_coins >= COIN_TO_COUP && opp_lives == 1);
    };
    
    // Fresh turn
    if (last_action == INCOME || last_action == PASS_BLOCK || 
        (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE)) {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {INCOME};
        // ENFORCE RULE: LIKELY #2
        if (opp_lives == 2) {
            // Do NOT FA when opponent has claimed DUKE
            if (is_p1 && num_p2_has_claimed_duke > 0) ;
            else if (!is_p1 && num_p1_has_claimed_duke > 0) ;
            else actions.push_back(FOREIGN_AID);

            actions.push_back(TAX);

            // Do NOT STEAL when opponent has claimed CAPTAIN
            if (is_p1 && num_p2_has_claimed_steal_blocker > 0) ;
            else if (!is_p1 && num_p1_has_claimed_steal_blocker > 0) ;
            else {
                if (opp_coins == 1) actions.push_back(STEAL1);
                else if (opp_coins >= 2) actions.push_back(STEAL2);
            }

            // Do NOT ASSASSINATE when opponent has claimed CONTESSA
            if (is_p1 && num_p2_has_claimed_contessa > 0) ;
            else if (!is_p1 && num_p1_has_claimed_contessa > 0) ;
            else if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        } 
        else {
            actions.push_back(FOREIGN_AID);
            actions.push_back(TAX);
            if (opp_coins == 1) actions.push_back(STEAL1);
            else if (opp_coins >= 2) actions.push_back(STEAL2);
            if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        }

        // ENFORCE RULE: MOST LIKELY #6
        if (my_lives + opp_lives == 4) {
            if (is_p1) {
                if (num_p2_has_allowed_tax || num_p2_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                } else if (num_p2_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            } else {
                if (num_p1_has_allowed_tax || num_p1_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                }
                else if (num_p1_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            }
        }
        
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        
        return actions;
    }
    // vs Foreign Aid
    if (last_action == FOREIGN_AID) {
        if (must_coup()) return {COUP};
        // ENFORCE RULE: MOST LIKELY #4 
        // NO TAX vs FOREIGN_AID
        std::vector<Action> actions = {INCOME};

        // ENFORCE RULE: LIKELY #2
        if (opp_lives == 2) {
            // Do NOT FA when opponent has claimed DUKE
            if (is_p1 && num_p2_has_claimed_duke > 0) ;
            else if (!is_p1 && num_p1_has_claimed_duke > 0) ;
            else actions.push_back(FOREIGN_AID);

            // Do NOT STEAL when opponent has claimed CAPTAIN
            if (is_p1 && num_p2_has_claimed_steal_blocker > 0) ;
            else if (!is_p1 && num_p1_has_claimed_steal_blocker > 0) ;
            else actions.push_back(STEAL2);

            // Do NOT ASSASSINATE when opponent has claimed CONTESSA
            if (is_p1 && num_p2_has_claimed_contessa > 0) ;
            else if (!is_p1 && num_p1_has_claimed_contessa > 0) ;
            else if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        } else {
            actions.push_back(FOREIGN_AID);
            actions.push_back(STEAL2);
            if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        }

        // ENFORCE RULE: MOST LIKELY #6
        if (my_lives + opp_lives == 4) {
            if (is_p1) {
                if (num_p2_has_allowed_tax || num_p2_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                } else if (num_p2_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            } else {
                if (num_p1_has_allowed_tax || num_p1_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                }
                else if (num_p1_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            }
        }

        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        // ENFORCE RULE: LIKELY #1
        const bool player_has_allowed_fa = is_p1 ? num_p1_has_allowed_foreign_aid : num_p2_has_allowed_foreign_aid;
        if (!player_has_allowed_fa) actions.push_back(BLOCK_FOREIGN_AID);
        return actions;
    }
    // vs Tax
    if (last_action == TAX) {
        if (must_coup()) return {COUP};
        // ENFORCE RULE: MOST LIKELY #5
        // NO FA vs TAX
        std::vector<Action> actions = {INCOME, TAX};

        // ENFORCE RULE: LKELY #2
        if (opp_lives == 2) {
            // Do NOT STEAL when opponent has claimed CAPTAIN
            if (is_p1 && num_p2_has_claimed_steal_blocker > 0) ;
            else if (!is_p1 && num_p1_has_claimed_steal_blocker > 0) ;
            else {
                actions.push_back(STEAL2);
            }
            // Do NOT ASSASSINATE when opponent has claimed CONTESSA
            if (is_p1 && num_p2_has_claimed_contessa > 0) ;
            else if (!is_p1 && num_p1_has_claimed_contessa > 0) ;
            else if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        } else {
            actions.push_back(STEAL2);
            if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        }

        // ENFORCE RULE: MOST LIKELY #6
        if (my_lives + opp_lives == 4) {
            if (is_p1) {
                if (num_p2_has_allowed_tax || num_p2_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                } else if (num_p2_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            } else {
                if (num_p1_has_allowed_tax || num_p1_has_allowed_steal) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                    actions.erase(std::remove(actions.begin(), actions.end(), FOREIGN_AID), actions.end());
                }
                else if (num_p1_has_allowed_foreign_aid) {
                    actions.erase(std::remove(actions.begin(), actions.end(), INCOME), actions.end());
                }
            }
        }

        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        actions.push_back(CHALLENGE);
        return actions;
    }
    // vs Steal 1
    if (last_action == STEAL1) {
        // ENFORCE RULE: LIKELY #1
        const bool player_has_has_allowed_steal = is_p1 ? num_p1_has_allowed_steal : num_p2_has_allowed_steal;
        // ENFORCE RULE: MOST LIKELY #2 (NO INCOME, FA, STEAL vs STEAL)
        if (!player_has_has_allowed_steal) return {TAX, BLOCK_STEAL1_AMB, BLOCK_STEAL1_CAP, CHALLENGE};
        return {TAX, CHALLENGE};
    }
    // vs Steal 2
    if (last_action == STEAL2) {
        if (must_coup()) return {COUP};
        
        // ENFORCE RULE: MOST LIKELY #2 (NO INCOME, FA, STEAL vs STEAL)
        std::vector<Action> actions = {TAX};

        // ENFORCE RULE: LIKELY #2
        if (opp_lives == 2) {
            // Do NOT ASSASSINATE when opponent has claimed CONTESSA
            if (is_p1 && num_p2_has_claimed_contessa > 0) ;
            else if (!is_p1 && num_p1_has_claimed_contessa > 0) ;
            else  {
                if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
            }
        } else {
            if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        }
        
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);

        // ENFORCE RULE: LIKELY #1 
        const bool player_has_has_allowed_steal = is_p1 ? num_p1_has_allowed_steal : num_p2_has_allowed_steal;
        if (!player_has_has_allowed_steal) {
            actions.push_back(BLOCK_STEAL2_AMB);
            actions.push_back(BLOCK_STEAL2_CAP);
        }
        actions.push_back(CHALLENGE);
        return actions;
    }
    
    // vs Assassinate
    if (last_action == ASSASSINATE) {
        // ENFORCE RULE: LIKELY #1
        const bool player_has_allowed_assa = is_p1 ? num_p1_has_allowed_assassinate : num_p2_has_allowed_assassinate;
        if (player_has_allowed_assa) return {CHALLENGE};
        std::vector<Action> actions = {BLOCK_ASSASSINATE, CHALLENGE};
        // RULE: DEFINITELY #2
        if (my_lives == 1) return actions;
        // RULE: MOST LIKELY #3
        if ((my_cards[0] == CONTESSA && my_influence[0] == 1) ||
            (my_cards[0] == CONTESSA && my_influence[1] == 1)) {
                return actions;
        }

        std::vector<Action> lose_actions = get_card_losing_actions(my_cards, my_influence);
        actions.insert(actions.end(), lose_actions.begin(), lose_actions.end());
        return actions;
    }
    
    // Block responses - challenge or pass
    if (last_action == BLOCK_FOREIGN_AID) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB || 
        last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_ASSASSINATE) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    // vs Coup
    if (last_action == COUP) {
        return get_card_losing_actions(my_cards, my_influence);
    }
    
    // vs Challenge
    if (last_action == CHALLENGE) {
        const Action challenged_action = history[history.size() - 2];
        
        // RULE: MOST LIKELY #1
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

std::vector<Action> GameState::get_original_legal_actions() const {
    if (is_terminal()) return {};
    if (history.empty()) return {INCOME, FOREIGN_AID, TAX, STEAL2};

    const Action last_action = history.back();
    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const int opp_coins = is_p1 ? p2_coins : p1_coins;
    const std::array<Card, 2> my_cards = is_p1 ? p1_cards : p2_cards;
    const std::array<int, 2> my_influence = is_p1 ? p1_influence : p2_influence;
    const int my_lives = my_influence[0] + my_influence[1];

    auto must_coup = [&]() -> bool {
        return my_coins >= COIN_TO_MUST_COUP; 
    };
    
    // Fresh turn
    if (last_action == INCOME || last_action == PASS_BLOCK || 
        (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE)) {
        if (must_coup()) return {COUP};
        std::vector<Action> actions = {INCOME, FOREIGN_AID, TAX};
        if (opp_coins == 1) actions.push_back(STEAL1);
        else if (opp_coins >= 2) actions.push_back(STEAL2);
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        return actions;
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
        if (current_player == 0 && p2_coins == 1)
            return {INCOME, FOREIGN_AID, TAX, STEAL1, BLOCK_STEAL1_AMB, BLOCK_STEAL1_CAP, CHALLENGE};
        if (current_player == 1 && p1_coins == 1)
            return {INCOME, FOREIGN_AID, TAX, STEAL1, BLOCK_STEAL1_AMB, BLOCK_STEAL1_CAP, CHALLENGE};
        return {INCOME, FOREIGN_AID, TAX, STEAL2, BLOCK_STEAL1_AMB, BLOCK_STEAL1_CAP, CHALLENGE};
    }
    // vs Steal 2
    if (last_action == STEAL2) {
        if (must_coup()) return {COUP};
        
        std::vector<Action> actions = {INCOME, FOREIGN_AID, TAX, STEAL2};
        if (my_coins >= COIN_TO_ASSASSINATE) actions.push_back(ASSASSINATE);
        if (my_coins >= COIN_TO_COUP) actions.push_back(COUP);
        actions.push_back(BLOCK_STEAL2_AMB);
        actions.push_back(BLOCK_STEAL2_CAP);
        actions.push_back(CHALLENGE);
        return actions;
    }
    
    // vs Assassinate
    if (last_action == ASSASSINATE) {
        std::vector<Action> actions = {BLOCK_ASSASSINATE, CHALLENGE};
        std::vector<Action> lose_actions = get_card_losing_actions(my_cards, my_influence);
        actions.insert(actions.end(), lose_actions.begin(), lose_actions.end());
        return actions;
    }
    
    // Block responses - challenge or pass
    if (last_action == BLOCK_FOREIGN_AID) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_STEAL1_AMB || last_action == BLOCK_STEAL2_AMB || 
        last_action == BLOCK_STEAL1_CAP || last_action == BLOCK_STEAL2_CAP) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    if (last_action == BLOCK_ASSASSINATE) {
        return {CHALLENGE, PASS_BLOCK};
    }
    
    // vs Coup
    if (last_action == COUP) {
        return get_card_losing_actions(my_cards, my_influence);
    }
    
    // vs Challenge
    if (last_action == CHALLENGE) {
        const Action challenged_action = history[history.size() - 2];
        std::vector<Action> actions;

        bool have_contessa = false;

        for (int i = 0; i < 2; i++) {
            if (my_influence[i] == 0) continue;
            const Card card = my_cards[i];
            if (card == DUKE && (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID)) {
                actions.push_back(SHOW_DUKE);
                break;
            }
            else if (card == ASSASSIN && challenged_action == ASSASSINATE) {
                actions.push_back(SHOW_ASSASSIN);
                break;
            }
            else if (card == CONTESSA && challenged_action == BLOCK_ASSASSINATE) {
                have_contessa = true;
                actions.push_back(SHOW_CONTESSA);
                break;
            }
            else if (card == CAPTAIN && (challenged_action == STEAL1 || challenged_action == STEAL2 || 
                                    challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP)) {
                actions.push_back(SHOW_CAPTAIN);
                break;
            }
            else if (card == AMBASSADOR && (challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL2_AMB)) {
                actions.push_back(SHOW_AMBASSADOR);
                break;
            }
        }
        
        // Check for double assassination special case
        if (my_lives == 2 && challenged_action == BLOCK_ASSASSINATE && !have_contessa) {
            return {LOSE_BOTH};
        }

        std::vector<Action> lose_actions = get_card_losing_actions(my_cards, my_influence);
        actions.insert(actions.end(), lose_actions.begin(), lose_actions.end());
        return actions;
    }
    
    // vs Show card
    if (last_action >= SHOW_ASSASSIN && last_action <= SHOW_DUKE) {
        // Check for double assassination special case
        if (my_lives == 2 && last_action == SHOW_ASSASSIN) {
            return {LOSE_BOTH};
        }
        return get_card_losing_actions(my_cards, my_influence);
    }
    assert(false && "Invalid state in get_original_legal_actions()");
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
}

void GameState::undo_lose_card(Card card) {
    const size_t hist_size = history.size();
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
    // FOR DEBUGGING
    // std::vector<Action> legal_actions = get_legal_actions();  // Store result first
    // if (std::find(legal_actions.begin(), legal_actions.end(), action) == legal_actions.end()) {
    //     std::cout << "---ERROR: ILLEGAL ACTION ATTEMPTED---" << std::endl;
    //     std::cout << "Attempted action: " << ACTION_NAMES[action] << std::endl; 
    //     std::cout << get_game_state();
    //     assert(false && "Illegal action applied.");
    // }

    history.push_back(action);
    
    const bool is_p1 = (current_player == 0);
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;

    std::array<int, 2>& my_influence = is_p1 ? p1_influence : p2_influence;
    
    // Apply changes coins and cards
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
            break;
            
        case BLOCK_ASSASSINATE:
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

    // APPLY RULE: LIKELY #2
    // If current action is card-claiming action
    // Increment current player's claim count
    switch (action) {
        case TAX:
        case BLOCK_FOREIGN_AID:
            if (is_p1) num_p1_has_claimed_duke++;
            else num_p2_has_claimed_duke++;
            break;
        case STEAL1:
        case STEAL2:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP:
            if (is_p1) num_p1_has_claimed_steal_blocker++;
            else num_p2_has_claimed_steal_blocker++;
            break;
        case BLOCK_ASSASSINATE:
            if (is_p1) num_p1_has_claimed_contessa++;
            else num_p2_has_claimed_contessa++;
            break;
        default:
            break;
    }

    // APPLY RULE: MOST LIKELY #6 + LIKELY #1
    // If current action allows previous STEAL, FA, ASSASINATE
    // Increment current player's num_allowed_action
    if (history.size() >= 2) {
        const Action prev_action = history[history.size() - 2];
        if (prev_action == FOREIGN_AID && action != BLOCK_FOREIGN_AID) {
            if (is_p1) num_p1_has_allowed_foreign_aid++;
            else num_p2_has_allowed_foreign_aid++;
        }
        else if (prev_action == STEAL1 && action != BLOCK_STEAL1_CAP && action != BLOCK_STEAL1_AMB && action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_steal++;
            else num_p2_has_allowed_steal++;
        }
        else if (prev_action == STEAL2 && action != BLOCK_STEAL2_CAP && action != BLOCK_STEAL2_AMB && action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_steal++;
            else num_p2_has_allowed_steal++;
        }
        else if (prev_action == ASSASSINATE && action != BLOCK_ASSASSINATE && action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_assassinate++;
            else num_p2_has_allowed_assassinate++;
        }
        else if (prev_action == TAX && action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_tax++;
            else num_p2_has_allowed_tax++;
        }
    }

    // APPLY RULE: LIKELY #2
    // When card-claiming action is challenged
    // Decrement opponent's claim count
    if (action == CHALLENGE) {
        const Action prev_action = history[history.size() - 2];
        switch (prev_action) {
            case TAX:
            case BLOCK_FOREIGN_AID:
                if (!is_p1) num_p1_has_claimed_duke--;
                else num_p2_has_claimed_duke--;
                break;
            case STEAL1:
            case STEAL2:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:
                if (!is_p1) num_p1_has_claimed_steal_blocker--;
                else num_p2_has_claimed_steal_blocker--;
                break;
            case BLOCK_ASSASSINATE:
                if (!is_p1) num_p1_has_claimed_contessa--;
                else num_p2_has_claimed_contessa--;
                break;
            default:
                break;
        }
    }
    
    // Turn correction (MUST BE DONE LAST)
    if (action >= LOSE_ASSASSIN && action <= LOSE_BOTH) {
        constexpr size_t MAX_ACTIONS_PER_TURN = 5;
        for (size_t i = 1; i <= MAX_ACTIONS_PER_TURN; i++) {
            const Action action = history[history.size() - i];
            // Check if it's a starting action (0-6)
            if (action <= COUP) {
                // Bad alternation happens when a turn has even # of moves 
                if (i % 2 == 0) {
                    current_player = 1 - current_player;
                }
                break;
            }
        }
    } 
    
    current_player = 1 - current_player;
}

void GameState::undo_action() {
    const size_t hist_size = history.size();
    assert(hist_size > 0);
    
    const Action last_action = history.back();
    // Turn correction (MUST BE DONE FIRST)
    if (last_action >= LOSE_ASSASSIN && last_action <= LOSE_BOTH) {
        for (size_t i = 1; i <= 5; i++) {
            const Action action = history[hist_size - i];
            // Check if it's a starting action (0-6)
            if (action <= COUP) {
                // Even # of actions means incorrect alternation
                if (i % 2 == 0) current_player = 1 - current_player;
                break;
            }
        }
    }
    
    history.pop_back();
    current_player = 1 - current_player;
    const bool is_p1 = (current_player == 0);
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
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
            break;
            
        case BLOCK_ASSASSINATE:
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
            break;
    }

    // UNDO RULE: MOST LIKELY #6 + LIKELY #1
    // If current action allows previous STEAL, FA, ASSASINATE
    // Decrement current player's num_allowed_action
    if (history.size() >= 1) {
        const Action prev_action = history[history.size() - 1];
        if (prev_action == FOREIGN_AID && last_action != BLOCK_FOREIGN_AID) {
            if (is_p1) num_p1_has_allowed_foreign_aid--;
            else num_p2_has_allowed_foreign_aid--;
        }
        else if (prev_action == STEAL1 && last_action != BLOCK_STEAL1_CAP && last_action != BLOCK_STEAL1_AMB && last_action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_steal--;
            else num_p2_has_allowed_steal--;
        }
        else if (prev_action == STEAL2 && last_action != BLOCK_STEAL2_CAP && last_action != BLOCK_STEAL2_AMB && last_action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_steal--;
            else num_p2_has_allowed_steal--;
        }
        else if (prev_action == ASSASSINATE && last_action != BLOCK_ASSASSINATE && last_action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_assassinate--;
            else num_p2_has_allowed_assassinate--;
        }
        else if (prev_action == TAX && last_action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_tax--;
            else num_p2_has_allowed_tax--;
        }
    }

    // UNDO RULE: LIKELY #2
    // If current action is card-claiming action
    // Decrement current player's claim count
    switch (last_action) {
        case TAX:
        case BLOCK_FOREIGN_AID:
            if (is_p1) num_p1_has_claimed_duke--;
            else num_p2_has_claimed_duke--;
            break;
        case STEAL1:
        case STEAL2:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP:
            if (is_p1) num_p1_has_claimed_steal_blocker--;
            else num_p2_has_claimed_steal_blocker--;
            break;
        case BLOCK_ASSASSINATE:
            if (is_p1) num_p1_has_claimed_contessa--;
            else num_p2_has_claimed_contessa--;
            break;
        default:
            break;
    }

    // UNDO RULE: LIKELY #2
    // When card-claiming action is challenged
    // Increment opponent's claim count
    if (last_action == CHALLENGE) {
        const Action prev_action = history[history.size() - 1];
        switch (prev_action) {
            case TAX:
            case BLOCK_FOREIGN_AID:
                if (!is_p1) num_p1_has_claimed_duke++;
                else num_p2_has_claimed_duke++;
                break;
            case STEAL1:
            case STEAL2:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:
                if (!is_p1) num_p1_has_claimed_steal_blocker++;
                else num_p2_has_claimed_steal_blocker++;
                break;
            case BLOCK_ASSASSINATE:
                if (!is_p1) num_p1_has_claimed_contessa++;
                else num_p2_has_claimed_contessa++;
                break;
            default:
                break;
        }
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
    std::string result;
    result.reserve(50 + history.size() * 15);
    
    result.append(CARD_NAMES[my_cards[0]]);
    result.push_back(' ');
    result.append(CARD_NAMES[my_cards[1]]);
    result.append(": ");
    
    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) result.append(", ");
        result.append(ACTION_NAMES[history[i]]);
    }
    
    return result;
}

// FOR DEBUGGING
void GameState::print_history() const {
    std::cout << "History: ";
    for (Action a : history) {
        std::cout << ACTION_NAMES[a] << " ";
    }
    std::cout << std::endl;
}

void GameState::print_game_state() const {
    std::cout << "Current player: " << current_player << std::endl;
    std::cout << "P1 Cards: " << p1_cards[0] << (p1_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p1_cards[1] << (p1_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;
    std::cout << "P2 Cards: " << p2_cards[0] << (p2_influence[0] == 1 ? "(ALIVE) " : "(DEAD) ") <<
                 p2_cards[1] << (p2_influence[1] == 1 ? "(ALIVE)" : "(DEAD)") << std::endl;

    std::cout << "P1 Coins: " << p1_coins << std::endl;
    std::cout << "P2 Coins: " << p2_coins << std::endl;

    print_history();
    
    std::cout << "Possible actions: ";
    for (Action a : get_legal_actions()) {
        std::cout << ACTION_NAMES[a] << " ";
    }
    std::cout << std::endl;
    std::cout << std::endl;
}

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

    output_string += "\nLIKELY RULE#1 count variables: ";
    output_string += std::to_string(num_p1_has_allowed_foreign_aid) + " ";
    output_string += std::to_string(num_p1_has_allowed_steal) + " ";
    output_string += std::to_string(num_p1_has_allowed_assassinate) + " ";
    output_string += std::to_string(num_p2_has_allowed_foreign_aid) + " ";
    output_string += std::to_string(num_p2_has_allowed_steal) + " ";
    output_string += std::to_string(num_p2_has_allowed_assassinate) + " ";

    output_string += "\nLIKELY RULE#2 count variables: ";
    output_string += std::to_string(num_p1_has_claimed_duke) + " ";
    output_string += std::to_string(num_p1_has_claimed_steal_blocker) + " ";
    output_string += std::to_string(num_p1_has_claimed_contessa) + " ";
    output_string += std::to_string(num_p2_has_claimed_duke) + " ";
    output_string += std::to_string(num_p2_has_claimed_steal_blocker) + " ";
    output_string += std::to_string(num_p2_has_claimed_contessa) + " ";

    output_string += "\n\n";
    
    return output_string;
}