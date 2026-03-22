#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

static inline ActionMask action_bit(Action a) noexcept {
    return (static_cast<ActionMask>(1u) << static_cast<unsigned>(a));
}

RulesConfig RulesConfig::solver_default() {
    return RulesConfig{};
}

RulesConfig RulesConfig::baseline_default() {
    RulesConfig config;
    config.pruning.enabled = false;
    config.extensions.claim_mate_enabled = false;
    return config;
}

GameState::GameState() {
    rules_config = RulesConfig::solver_default();
    reset();
}

GameState::GameState(const RulesConfig& config) : rules_config(config) {
    reset();
}

void GameState::reset() {
    current_player = 0;
    p1_cards = {ASSASSIN, ASSASSIN};
    p2_cards = {ASSASSIN, ASSASSIN};
    p1_influence = {1,1};
    p2_influence = {1,1};
    p1_coins = 2;
    p2_coins = 2;
    history = {};
    history.reserve(50);

    // Pruning heuristic state: prior accepted stronger actions.
    num_p1_has_allowed_tax = 0;
    num_p2_has_allowed_tax = 0;
    num_p1_has_allowed_block_fa = 0;
    num_p2_has_allowed_block_fa = 0;

    // Pruning heuristic state: prior accepted actions.
    num_p1_has_allowed_foreign_aid = 0;
    num_p2_has_allowed_foreign_aid = 0;
    num_p1_has_allowed_steal = 0;
    num_p2_has_allowed_steal = 0;
    num_p1_has_allowed_assassinate = 0;
    num_p2_has_allowed_assassinate = 0;

    // Pruning heuristic state: prior public claims.
    num_p1_has_claimed_duke = 0;
    num_p2_has_claimed_duke = 0;
    num_p1_has_claimed_steal_blocker = 0;
    num_p2_has_claimed_steal_blocker = 0;
    num_p1_has_claimed_contessa = 0;
    num_p2_has_claimed_contessa = 0;

    // Pruning heuristic snapshots.
    p1_claims_duke_at_first_loss = -1;
    p1_claims_steal_blocker_at_first_loss = -1;
    p1_claims_contessa_at_first_loss = -1;
    p2_claims_duke_at_first_loss = -1;
    p2_claims_steal_blocker_at_first_loss = -1;
    p2_claims_contessa_at_first_loss = -1;
}


bool GameState::is_terminal() const {
    if ((p1_influence[0] == 0 && p1_influence[1] == 0) ||
        (p2_influence[0] == 0 && p2_influence[1] == 0))
        return true;
    return (!history.empty() && history.back() == CLAIM_MATE);
}

double GameState::get_utility() const {
    assert(is_terminal() && "get_utility() called on a non-terminal game state");

    // Previous player played CLAIM_MATE -> current player loses
    if (!history.empty() && history.back() == CLAIM_MATE) {
        return -1.0;
    }

    const bool p1_dead = (p1_influence[0] == 0 && p1_influence[1] == 0);
    const bool p2_dead = (p2_influence[0] == 0 && p2_influence[1] == 0);

    if (current_player == 0) {
        return p2_dead ? 1.0 : -1.0;
    } else {
        return p1_dead ? 1.0 : -1.0;
    }

    assert(false && "get_utility() unexpectedly reached end of function");
}
   
double GameState::get_br_utility(int maximizing_player, std::array<double, NUM_HOLDINGS> cards_distribution) const {
    assert(is_terminal() && "get_br_utility() called on a non-terminal game state");
    
    // CLAIM_MATE means current player loses
    if (!history.empty() && history.back() == CLAIM_MATE) {
        return -1.0;
    }

    const size_t hist_size = history.size();
    assert(history.size() >= 2 && "get_br_utility() called on a non-terminal state with size < 2");

    const Action prev_action = history[hist_size - 2];
    // Coup
    if (prev_action == COUP) return 1.0;
    // Challenge
    const bool prev_was_challenge = (prev_action == CHALLENGE);
    const size_t challenged_idx = prev_was_challenge ? (hist_size - 3) : (hist_size - 4);
    const Action challenged_action = history[challenged_idx];

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
        if ((current_influence[0] == 1 && current_cards[0] == challenged_card) ||
            (current_influence[1] == 1 && current_cards[1] == challenged_card)) {
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

int GameState::get_current_player() const {
    return current_player;
}

void GameState::set_rules_config(const RulesConfig& config) {
    rules_config = config;
}

const RulesConfig& GameState::get_rules_config() const {
    return rules_config;
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

// Return true if current player can achieve coup-mate, false otherwise
bool GameState::can_2v1_coupmate(int my_coins, int opp_coins, const std::array<Card, 2>& my_cards) const {
    // DUKE @
    if (my_cards[1] == DUKE) {
        if (my_coins >= 4) {
            return true;
        }
        else if (my_coins == 3 && opp_coins <= 5) {
            return true;
        }
        else if (my_coins == 2 && opp_coins <= 3) {
            return true;
        }
        else if (my_coins == 1 && opp_coins <= 1) {
            return true;
        }
    }
    
    // DUKE CONTESSA
    if (my_cards[0] == CONTESSA && my_cards[1] == DUKE) {
        if (my_coins == 3 && opp_coins <= 9) {
            return true;
        }
        else if (my_coins == 2 && opp_coins <= 7) {
            return true;
        }
        else if (my_coins == 1 && opp_coins <= 5) {
            return true;
        }
        else if (opp_coins <= 3) {
            return true;
        }
    }
    
    // DUKE SB (CAPTAIN or AMBASSADOR)
    if ((my_cards[0] == CAPTAIN && my_cards[1] == DUKE) ||
        (my_cards[0] == AMBASSADOR && my_cards[1] == DUKE)) {
        if (opp_coins <= 2) {
            return true;
        }
    }
    
    // STEAL BLOCKER (CAPTAIN or AMBASSADOR) @
    if (my_cards[0] == CAPTAIN || my_cards[1] == CAPTAIN ||
        my_cards[0] == AMBASSADOR || my_cards[1] == AMBASSADOR) {
        if (my_coins >= 6) {
            return true;
        }
        else if (my_coins == 5 && opp_coins <= 5) {
            return true;
        }
        else if (my_coins == 4 && opp_coins <= 2) {
            return true;
        }
    }
    
    // SB CONTESSA
    if ((my_cards[0] == CAPTAIN && my_cards[1] == CONTESSA) ||
        (my_cards[0] == AMBASSADOR && my_cards[1] == CONTESSA)) {
        if (my_coins == 5 && opp_coins <= 9) {
            return true;
        }
        else if (my_coins == 4 && opp_coins <= 6) {
            return true;
        }
        else if (my_coins == 3 && opp_coins <= 3) {
            return true;
        }
        else if (my_coins == 2 && opp_coins == 0) {
            return true;
        }
    }

    return false;
}

// Pruning helper: prior accepted stronger actions.
bool GameState::has_opponent_allowed_tax() const {
    if (current_player == 0) {
        return num_p2_has_allowed_tax > 0;
    } else {
        return num_p1_has_allowed_tax > 0;
    }   
}

bool GameState::has_opponent_allowed_foreign_aid() const {
    if (current_player == 0) {
        return num_p2_has_allowed_foreign_aid > 0;
    } else {
        return num_p1_has_allowed_foreign_aid > 0;
    }   
}

bool GameState::has_opponent_allowed_steal() const {
    if (current_player == 0) {
        return num_p2_has_allowed_steal > 0;
    } else {
        return num_p1_has_allowed_steal > 0;
    }   
}

// Pruning helper: prior accepted actions.
bool GameState::has_allowed_foreign_aid() const {
    if (current_player == 0) {
        return num_p1_has_allowed_foreign_aid > 0;
    } else {
        return num_p2_has_allowed_foreign_aid > 0;
    }
}

// Pruning helper: prior accepted actions.
bool GameState::has_allowed_steal() const {
    if (current_player == 0) {
        return num_p1_has_allowed_steal > 0;
    } else {
        return num_p2_has_allowed_steal > 0;
    }
}

// Pruning helper: prior accepted actions.
bool GameState::has_allowed_assassinate() const {
    if (current_player == 0) {
        return num_p1_has_allowed_assassinate > 0;
    } else {
        return num_p2_has_allowed_assassinate > 0;
    }
}   

// Pruning helper: public claim history in 2v2.
bool GameState::has_opponent_claimed_duke_2v2(bool is_p1) const {
    if (is_p1) {
        return num_p2_has_claimed_duke > 0;
    } else {
        return num_p1_has_claimed_duke > 0;
    }
}

// Pruning helper: public claim history in 2v2.
bool GameState::has_opponent_claimed_steal_blocker_2v2(bool is_p1) const {
    if (is_p1) {
        return num_p2_has_claimed_steal_blocker > 0;
    } else {
        return num_p1_has_claimed_steal_blocker > 0;
    }
}

// Pruning helper: public claim history in 2v2.
bool GameState::has_opponent_claimed_contessa_2v2(bool is_p1) const {
    if (is_p1) {
        return num_p2_has_claimed_contessa > 0;
    } else {
        return num_p1_has_claimed_contessa > 0;
    }
}

// Pruning helper: post-first-loss claim history.
bool GameState::has_opponent_claimed_duke_xv1(bool is_p1) const {
    if (is_p1) {
        return (p2_claims_duke_at_first_loss >= 0 && 
                num_p2_has_claimed_duke > p2_claims_duke_at_first_loss);
    } else {
        return (p1_claims_duke_at_first_loss >= 0 && 
                num_p1_has_claimed_duke > p1_claims_duke_at_first_loss);
    }
}

// Pruning helper: post-first-loss claim history.
bool GameState::has_opponent_claimed_steal_blocker_xv1(bool is_p1) const {
    if (is_p1) {
        return (p2_claims_steal_blocker_at_first_loss >= 0 && 
                num_p2_has_claimed_steal_blocker > p2_claims_steal_blocker_at_first_loss);
    } else {
        return (p1_claims_steal_blocker_at_first_loss >= 0 && 
                num_p1_has_claimed_steal_blocker > p1_claims_steal_blocker_at_first_loss);
    }
}

// Pruning helper: post-first-loss claim history.
bool GameState::has_opponent_claimed_contessa_xv1(bool is_p1) const {
    if (is_p1) {
        return (p2_claims_contessa_at_first_loss >= 0 && 
                num_p2_has_claimed_contessa > p2_claims_contessa_at_first_loss);
    } else {
        return (p1_claims_contessa_at_first_loss >= 0 && 
                num_p1_has_claimed_contessa > p1_claims_contessa_at_first_loss);
    }
}

bool GameState::is_free_turn() const {
    if (history.empty()) return true;
    const Action last_action = history.back();
    return (last_action <= STEAL2 || last_action == PASS_BLOCK || 
            (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE));
}

void GameState::add_primary_turn_actions(ActionMask& legal_mask, int my_coins, int opp_coins) const {
    legal_mask |= action_bit(INCOME);
    legal_mask |= action_bit(FOREIGN_AID);
    legal_mask |= action_bit(TAX);
    if (opp_coins >= 2) legal_mask |= action_bit(STEAL2);
    else if (opp_coins == 1) legal_mask |= action_bit(STEAL1);
    if (my_coins >= COIN_TO_ASSASSINATE) legal_mask |= action_bit(ASSASSINATE);
    if (my_coins >= COIN_TO_COUP) legal_mask |= action_bit(COUP);
}

ActionMask GameState::build_baseline_legal_mask() const {
    if (is_terminal()) return 0;

    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const int opp_coins = is_p1 ? p2_coins : p1_coins;
    const auto& my_cards = is_p1 ? p1_cards : p2_cards;
    const auto& my_influence = is_p1 ? p1_influence : p2_influence;

    if (history.empty()) {
        ActionMask legal_mask = 0;
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
        return legal_mask;
    }

    const Action last_action = history.back();
    ActionMask legal_mask = 0;

    if (last_action == INCOME || last_action == PASS_BLOCK ||
        (last_action >= LOSE_ASSASSIN && last_action <= LOSE_DUKE)) {
        if (my_coins >= COIN_TO_MUST_COUP) return action_bit(COUP);
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
    }
    else if (last_action == FOREIGN_AID) {
        if (my_coins >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(BLOCK_FOREIGN_AID);
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
        legal_mask |= action_bit(BLOCK_FOREIGN_AID);
    }
    else if (last_action == TAX) {
        if (my_coins >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(CHALLENGE);
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
        legal_mask |= action_bit(CHALLENGE);
    }
    else if (last_action == STEAL1) {
        // This engine does not model an explicit "allow steal" action or a formal phase enum.
        // By design, taking a normal turn action here also serves as implicitly allowing the steal.
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
        legal_mask |= action_bit(BLOCK_STEAL1_AMB);
        legal_mask |= action_bit(BLOCK_STEAL1_CAP);
        legal_mask |= action_bit(CHALLENGE);
    }
    else if (last_action == STEAL2) {
        if (my_coins >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(CHALLENGE);
        // This engine does not model an explicit "allow steal" action or a formal phase enum.
        // By design, taking a normal turn action here also serves as implicitly allowing the steal.
        add_primary_turn_actions(legal_mask, my_coins, opp_coins);
        legal_mask |= action_bit(BLOCK_STEAL2_AMB);
        legal_mask |= action_bit(BLOCK_STEAL2_CAP);
        legal_mask |= action_bit(CHALLENGE);
    }
    else if (last_action == ASSASSINATE) {
        const int my_num_lives = my_influence[0] + my_influence[1];
        if (my_num_lives == 1) {
            return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
        }

        const bool has_live_contessa =
            (my_cards[0] == CONTESSA && my_influence[0] > 0) ||
            (my_cards[1] == CONTESSA && my_influence[1] > 0);
        if (has_live_contessa) {
            return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
        }

        legal_mask |= action_bit(BLOCK_ASSASSINATE);
        legal_mask |= action_bit(CHALLENGE);
        set_card_losing_bits(my_cards, my_influence, legal_mask);
    }
    else if (last_action >= BLOCK_FOREIGN_AID && last_action <= BLOCK_ASSASSINATE) {
        return action_bit(CHALLENGE) | action_bit(PASS_BLOCK);
    }
    else if (last_action == COUP) {
        set_card_losing_bits(my_cards, my_influence, legal_mask);
    }
    else if (last_action == CHALLENGE) {
        const Action challenged_action = history[history.size() - 2];

        for (int i = 0; i < 2; i++) {
            if (my_influence[i] == 0) continue;
            const Card card = my_cards[i];
            if (card == DUKE && (challenged_action == TAX || challenged_action == BLOCK_FOREIGN_AID)) {
                return action_bit(SHOW_DUKE);
            }
            if (card == ASSASSIN && challenged_action == ASSASSINATE) {
                return action_bit(SHOW_ASSASSIN);
            }
            if (card == CONTESSA && challenged_action == BLOCK_ASSASSINATE) {
                return action_bit(SHOW_CONTESSA);
            }
            if (card == CAPTAIN && (challenged_action == STEAL1 || challenged_action == STEAL2 ||
                                    challenged_action == BLOCK_STEAL1_CAP || challenged_action == BLOCK_STEAL2_CAP)) {
                return action_bit(SHOW_CAPTAIN);
            }
            if (card == AMBASSADOR && (challenged_action == BLOCK_STEAL1_AMB || challenged_action == BLOCK_STEAL2_AMB)) {
                return action_bit(SHOW_AMBASSADOR);
            }
        }

        if ((my_influence[0] + my_influence[1]) == 2 && challenged_action == BLOCK_ASSASSINATE) {
            return action_bit(LOSE_BOTH);
        }

        set_card_losing_bits(my_cards, my_influence, legal_mask);
    }
    else if (last_action >= SHOW_ASSASSIN && last_action <= SHOW_DUKE) {
        if ((my_influence[0] + my_influence[1]) == 2 && last_action == SHOW_ASSASSIN) {
            return action_bit(LOSE_BOTH);
        }
        set_card_losing_bits(my_cards, my_influence, legal_mask);
    }

    return legal_mask;
}

bool GameState::can_force_coup_vs_one_influence() const {
    if (!is_free_turn()) return false;

    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const auto& opp_influence = is_p1 ? p2_influence : p1_influence;
    const int opp_num_lives = opp_influence[0] + opp_influence[1];
    return my_coins >= COIN_TO_COUP && opp_num_lives == 1;
}

bool GameState::can_force_claim_mate() const {
    if (!rules_config.extensions.claim_mate_enabled || !is_free_turn()) return false;

    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const int opp_coins = is_p1 ? p2_coins : p1_coins;
    const auto& my_cards = is_p1 ? p1_cards : p2_cards;
    const auto& my_influence = is_p1 ? p1_influence : p2_influence;
    const auto& opp_influence = is_p1 ? p2_influence : p1_influence;

    const int my_num_lives = my_influence[0] + my_influence[1];
    const int opp_num_lives = opp_influence[0] + opp_influence[1];
    return my_num_lives == 2 && opp_num_lives == 1 && can_2v1_coupmate(my_coins, opp_coins, my_cards);
}

ActionMask GameState::apply_pruning_heuristics(ActionMask legal_mask) const {
    if (!rules_config.pruning.enabled || legal_mask == 0) return legal_mask;

    const bool is_p1 = (current_player == 0);
    const int my_coins = is_p1 ? p1_coins : p2_coins;
    const auto& my_cards = is_p1 ? p1_cards : p2_cards;
    const auto& my_influence = is_p1 ? p1_influence : p2_influence;
    const auto& opp_influence = is_p1 ? p2_influence : p1_influence;
    const int my_num_lives = my_influence[0] + my_influence[1];
    const int opp_num_lives = opp_influence[0] + opp_influence[1];

    if (can_force_coup_vs_one_influence()) {
        return action_bit(COUP);
    }

    if (can_force_claim_mate()) {
        return action_bit(CLAIM_MATE);
    }

    if (!history.empty()) {
        const Action last_action = history.back();

        if (last_action == FOREIGN_AID) {
            legal_mask &= ~action_bit(TAX);
            if (has_allowed_foreign_aid()) legal_mask &= ~action_bit(BLOCK_FOREIGN_AID);
        }
        else if (last_action == TAX) {
            legal_mask &= ~action_bit(FOREIGN_AID);
        }
        else if (last_action == STEAL1) {
            legal_mask &= ~action_bit(INCOME);
            legal_mask &= ~action_bit(FOREIGN_AID);
            legal_mask &= ~action_bit(STEAL1);
            legal_mask &= ~action_bit(STEAL2);
            legal_mask &= ~action_bit(ASSASSINATE);
            legal_mask &= ~action_bit(COUP);
            if (has_allowed_steal()) {
                legal_mask &= ~action_bit(BLOCK_STEAL1_AMB);
                legal_mask &= ~action_bit(BLOCK_STEAL1_CAP);
            }
        }
        else if (last_action == STEAL2) {
            legal_mask &= ~action_bit(INCOME);
            legal_mask &= ~action_bit(FOREIGN_AID);
            legal_mask &= ~action_bit(STEAL1);
            legal_mask &= ~action_bit(STEAL2);
            if (my_coins >= COIN_TO_MUST_COUP) {
                legal_mask &= ~action_bit(ASSASSINATE);
            }
            if (has_allowed_steal()) {
                legal_mask &= ~action_bit(BLOCK_STEAL2_AMB);
                legal_mask &= ~action_bit(BLOCK_STEAL2_CAP);
            }
        }
        else if (last_action == ASSASSINATE) {
            if (my_num_lives == 1) {
                return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
            }

            const bool has_live_contessa =
                (my_cards[0] == CONTESSA && my_influence[0] > 0) ||
                (my_cards[1] == CONTESSA && my_influence[1] > 0);
            if (has_live_contessa) {
                return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
            }

            if (has_allowed_assassinate()) {
                legal_mask &= ~action_bit(BLOCK_ASSASSINATE);
            }
        }
    }

    if (my_num_lives == 2 && opp_num_lives == 2) {
        if (has_opponent_claimed_steal_blocker_2v2(is_p1)) {
            legal_mask &= ~action_bit(STEAL1);
            legal_mask &= ~action_bit(STEAL2);
        }
        if (has_opponent_claimed_duke_2v2(is_p1)) {
            legal_mask &= ~action_bit(FOREIGN_AID);
        }
        if (has_opponent_claimed_contessa_2v2(is_p1)) {
            legal_mask &= ~action_bit(ASSASSINATE);
        }
    }

    if (opp_num_lives == 1) {
        if (has_opponent_claimed_steal_blocker_xv1(is_p1)) {
            legal_mask &= ~action_bit(STEAL1);
            legal_mask &= ~action_bit(STEAL2);
        }
        if (has_opponent_claimed_duke_xv1(is_p1)) {
            legal_mask &= ~action_bit(FOREIGN_AID);
        }
        if (has_opponent_claimed_contessa_xv1(is_p1)) {
            legal_mask &= ~action_bit(ASSASSINATE);
        }
    }

    if (my_num_lives == 2 && opp_num_lives == 2) {
        if (has_opponent_allowed_tax() && ((legal_mask & action_bit(TAX)) != 0)) {
            legal_mask &= ~action_bit(FOREIGN_AID);
            legal_mask &= ~action_bit(INCOME);
        }
        else if (has_opponent_allowed_steal() && ((legal_mask & action_bit(STEAL2)) != 0)) {
            legal_mask &= ~action_bit(FOREIGN_AID);
            legal_mask &= ~action_bit(INCOME);
        }
        else if (has_opponent_allowed_steal() && ((legal_mask & action_bit(STEAL1)) != 0)) {
            legal_mask &= ~action_bit(INCOME);
        }
        else if (has_opponent_allowed_foreign_aid() && ((legal_mask & action_bit(FOREIGN_AID)) != 0)) {
            legal_mask &= ~action_bit(INCOME);
        }
    }

    return legal_mask;
}

ActionMask GameState::add_extension_actions(ActionMask legal_mask) const {
    if (rules_config.extensions.claim_mate_enabled && can_force_claim_mate()) {
        legal_mask |= action_bit(CLAIM_MATE);
    }
    return legal_mask;
}

std::vector<Action> GameState::actions_from_mask(ActionMask legal_mask) const {
    std::vector<Action> legal_actions;
    for (unsigned a = 0; a < NUM_ACTIONS; a++) {
        if (legal_mask & action_bit(static_cast<Action>(a))) {
            legal_actions.push_back(static_cast<Action>(a));
        }
    }
    return legal_actions;
}

std::vector<Action> GameState::get_legal_actions() const {
    ActionMask legal_mask = build_baseline_legal_mask();
    legal_mask = apply_pruning_heuristics(legal_mask);
    legal_mask = add_extension_actions(legal_mask);
    return actions_from_mask(legal_mask);
}

void GameState::set_card_losing_bits(const std::array<Card, 2>& player_cards,
                                     const std::array<int, 2>& player_influence,
                                     ActionMask& legal_mask) const {
    // Build mask of lose actions and OR into provided legal_mask
    ActionMask m = 0;
    auto card_to_lose_bit = [&](Card c) -> ActionMask {
        switch (c) {
            case ASSASSIN:   return action_bit(LOSE_ASSASSIN);
            case AMBASSADOR: return action_bit(LOSE_AMBASSADOR);
            case CAPTAIN:    return action_bit(LOSE_CAPTAIN);
            case CONTESSA:   return action_bit(LOSE_CONTESSA);
            case DUKE:       return action_bit(LOSE_DUKE);
            default:         assert(false && "Invalid card type");
        }
    };

    if (player_influence[0] == 1) {
        m |= card_to_lose_bit(player_cards[0]);
    }

    if (player_influence[1] == 1) {
        const bool different_cards = (player_cards[0] != player_cards[1]);
        const bool same_card_but_first_dead = (player_cards[0] == player_cards[1] && player_influence[0] == 0);
        if (different_cards || same_card_but_first_dead) {
            m |= card_to_lose_bit(player_cards[1]);
        }
    }

    legal_mask |= m;
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

    // Capture pruning snapshot when a player loses first influence.
    int my_num_lives = my_influence[0] + my_influence[1];
    if (my_num_lives == 1) { // Just lost first influence (going from 2 to 1 life)
        if (is_p1) {
            p1_claims_duke_at_first_loss = num_p1_has_claimed_duke;
            p1_claims_steal_blocker_at_first_loss = num_p1_has_claimed_steal_blocker;
            p1_claims_contessa_at_first_loss = num_p1_has_claimed_contessa;
        } else {
            p2_claims_duke_at_first_loss = num_p2_has_claimed_duke;
            p2_claims_steal_blocker_at_first_loss = num_p2_has_claimed_steal_blocker;
            p2_claims_contessa_at_first_loss = num_p2_has_claimed_contessa;
        }
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

    // Reset pruning snapshot when undoing first influence loss.
    int my_num_lives = my_influence[0] + my_influence[1];
    if (my_num_lives == 2) { // Just restored first influence (going from 1 to 2 lives)
        if (is_p1) {
            p1_claims_duke_at_first_loss = -1;
            p1_claims_steal_blocker_at_first_loss = -1;
            p1_claims_contessa_at_first_loss = -1;
        } else {
            p2_claims_duke_at_first_loss = -1;
            p2_claims_steal_blocker_at_first_loss = -1;
            p2_claims_contessa_at_first_loss = -1;
        }
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

void GameState::apply_baseline_action(Action action) {
    const bool is_p1 = (current_player == 0);
    int& my_coins = is_p1 ? p1_coins : p2_coins;
    int& opp_coins = is_p1 ? p2_coins : p1_coins;
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
            my_influence[0] = 0;
            my_influence[1] = 0;
            break;

        default:
            // Actions that don't modify baseline state: CHALLENGE, PASS_BLOCK, SHOW_*
            break;
    }
}

void GameState::apply_extension_action(Action action) {
    if (action == CLAIM_MATE) {
        assert(rules_config.extensions.claim_mate_enabled && "CLAIM_MATE used while extension is disabled");
    }
}

void GameState::do_action(Action action) {
    history.push_back(action);
    apply_baseline_action(action);
    apply_extension_action(action);

    const bool is_p1 = (current_player == 0);

    // Update pruning claim trackers.
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

    // Update pruning "allowed action" trackers.
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
        else if (prev_action == BLOCK_FOREIGN_AID && action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_block_fa++;
            else num_p2_has_allowed_block_fa++;
        }
    }

    // Update pruning claim trackers on challenge.
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

void GameState::undo_baseline_action(Action last_action) {
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
}

void GameState::undo_extension_action(Action last_action) {
    if (last_action == CLAIM_MATE) {
        assert(rules_config.extensions.claim_mate_enabled && "CLAIM_MATE undone while extension is disabled");
    }
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
    undo_baseline_action(last_action);
    undo_extension_action(last_action);

    const bool is_p1 = (current_player == 0);

    // Undo pruning "allowed action" trackers.
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
        else if (prev_action == BLOCK_FOREIGN_AID && last_action != CHALLENGE) {
            if (is_p1) num_p1_has_allowed_block_fa--;
            else num_p2_has_allowed_block_fa--;
        }
    }

    // Undo pruning claim trackers.
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

    // Undo pruning claim trackers on challenge.
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

size_t GameState::get_history_hash(const std::vector<Action>& hist) {
    // FNV-1a hash algorithm - fast and good distribution
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    constexpr size_t FNV_PRIME = 1099511628211ULL;
    
    // Random salt to differentiate from infoset hash
    hash ^= static_cast<size_t>(10);
    hash *= FNV_PRIME;
    hash ^= static_cast<size_t>(10);
    hash *= FNV_PRIME;
    
    for (const Action action : hist) {
        hash ^= static_cast<size_t>(action);
        hash *= FNV_PRIME;
    }
    
    return hash;
}

size_t GameState::get_history_hash() const {
    return get_history_hash(history);
}

size_t GameState::get_infoset_hash() const {
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
    

    static const char* const CARD_NAMES[] = {
        "ASSASSIN", "AMBASSADOR", "CAPTAIN", "CONTESSA", "DUKE"
    };

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

void GameState::print_history() const {
    std::cout << "History: ";
    for (Action a : history) {
        std::cout << ACTION_NAMES[a] << " ";
    }
    std::cout << std::endl;
}

void GameState::print_game_state() const {
    std::cout << get_game_state();
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

    output_string += "\nPruning accepted-action counters: ";
    output_string += std::to_string(num_p1_has_allowed_foreign_aid) + " ";
    output_string += std::to_string(num_p1_has_allowed_steal) + " ";
    output_string += std::to_string(num_p1_has_allowed_assassinate) + " ";
    output_string += std::to_string(num_p2_has_allowed_foreign_aid) + " ";
    output_string += std::to_string(num_p2_has_allowed_steal) + " ";
    output_string += std::to_string(num_p2_has_allowed_assassinate) + " ";

    output_string += "\nPruning claim counters: ";
    output_string += std::to_string(num_p1_has_claimed_duke) + " ";
    output_string += std::to_string(num_p1_has_claimed_steal_blocker) + " ";
    output_string += std::to_string(num_p1_has_claimed_contessa) + " ";
    output_string += std::to_string(num_p2_has_claimed_duke) + " ";
    output_string += std::to_string(num_p2_has_claimed_steal_blocker) + " ";
    output_string += std::to_string(num_p2_has_claimed_contessa) + " ";

    output_string += "\n\n";
    
    return output_string;
}
