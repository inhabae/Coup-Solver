#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline ActionMask action_bit(Action a) noexcept {
    return (static_cast<ActionMask>(1u) << static_cast<unsigned>(a));
}

static inline bool is_block(Action a) {
    return a >= BLOCK_FOREIGN_AID && a <= BLOCK_ASSASSINATE;
}

static inline bool is_primary_action(Action a) {
    return a <= COUP;
}

// The card that must be held to legally play / block with this action.
// Returns ASSASSIN as a sentinel for actions that have no card requirement.
static Card card_claimed_by(Action a) {
    switch (a) {
        case TAX:
        case BLOCK_FOREIGN_AID:              return DUKE;
        case STEAL1:
        case STEAL2:
        case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_CAP:               return CAPTAIN;
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL2_AMB:               return AMBASSADOR;
        case ASSASSINATE:                    return ASSASSIN;
        case BLOCK_ASSASSINATE:              return CONTESSA;
        default:                             return ASSASSIN; // sentinel / unchallenged
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RulesConfig
// ─────────────────────────────────────────────────────────────────────────────

RulesConfig RulesConfig::solver_default()   { return RulesConfig{}; }

RulesConfig RulesConfig::baseline_default() {
    RulesConfig c;
    c.pruning.enabled              = false;
    c.extensions.claim_mate_enabled = false;
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / reset
// ─────────────────────────────────────────────────────────────────────────────

GameState::GameState() : rules_config(RulesConfig::solver_default()) { reset(); }
GameState::GameState(const RulesConfig& rc) : rules_config(rc)        { reset(); }

void GameState::reset() {
    phase          = Phase::ACTION;
    active_player  = 0;
    current_player = 0;
    pending_action = INCOME; // harmless sentinel

    p1_cards     = {ASSASSIN, ASSASSIN};
    p2_cards     = {ASSASSIN, ASSASSIN};
    p1_influence = {1, 1};
    p2_influence = {1, 1};
    p1_coins     = 2;
    p2_coins     = 2;

    history.clear();
    history.reserve(50);

    num_p1_has_allowed_tax           = 0;
    num_p2_has_allowed_tax           = 0;
    num_p1_has_allowed_block_fa      = 0;
    num_p2_has_allowed_block_fa      = 0;
    num_p1_has_allowed_foreign_aid   = 0;
    num_p2_has_allowed_foreign_aid   = 0;
    num_p1_has_allowed_steal         = 0;
    num_p2_has_allowed_steal         = 0;
    num_p1_has_allowed_assassinate   = 0;
    num_p2_has_allowed_assassinate   = 0;
    num_p1_has_claimed_duke          = 0;
    num_p2_has_claimed_duke          = 0;
    num_p1_has_claimed_steal_blocker = 0;
    num_p2_has_claimed_steal_blocker = 0;
    num_p1_has_claimed_contessa      = 0;
    num_p2_has_claimed_contessa      = 0;

    p1_claims_duke_at_first_loss          = -1;
    p1_claims_steal_blocker_at_first_loss = -1;
    p1_claims_contessa_at_first_loss      = -1;
    p2_claims_duke_at_first_loss          = -1;
    p2_claims_steal_blocker_at_first_loss = -1;
    p2_claims_contessa_at_first_loss      = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Terminal / utility
// ─────────────────────────────────────────────────────────────────────────────

bool GameState::is_terminal() const {
    return phase == Phase::TERMINAL;
}

double GameState::get_utility() const {
    assert(is_terminal());
    // current_player at terminal is the player who just *acted* (CLAIM_MATE) or
    // the survivor.  We want utility from the perspective of current_player.

    if (!history.empty() && history.back() == CLAIM_MATE) {
        // The player who played CLAIM_MATE wins; that is 1 - current_player
        // because do_action flips current_player after every action.
        return -1.0;
    }

    const bool p1_dead = (p1_influence[0] == 0 && p1_influence[1] == 0);
    const bool p2_dead = (p2_influence[0] == 0 && p2_influence[1] == 0);

    // current_player is the player about to act (post-flip), so:
    return (current_player == 0) ? (p2_dead ? 1.0 : -1.0)
                                 : (p1_dead ? 1.0 : -1.0);
}

double GameState::get_br_utility(int max_player,
                                  std::array<double, NUM_HOLDINGS> dist) const {
    assert(is_terminal());

    if (!history.empty() && history.back() == CLAIM_MATE) return -1.0;

    // Coup: the player who was couped loses. That player is current_player
    // (the one asked to lose a card) — but current_player was flipped after COUP
    // so it's actually the opponent of the coup-initiator. Simpler: if the
    // terminal was a COUP then the initiator (1-current_player) wins.
    if (!history.empty() && history.back() == COUP) return 1.0;

    // Challenge resolution: pending_action holds the challenged action.
    // If max_player is challenged, check their actual cards.
    // If the opponent is challenged, weight over card distribution.
    const Card needed = card_claimed_by(pending_action);

    // max_player is the one who was challenged
    if (current_player == max_player) {
        const auto& cards = (max_player == 0) ? p1_cards : p2_cards;
        const auto& inf   = (max_player == 0) ? p1_influence : p2_influence;
        const bool  has   = (inf[0] == 1 && cards[0] == needed) ||
                            (inf[1] == 1 && cards[1] == needed);
        return has ? 1.0 : -1.0;
    } else {
        double u = 0.0;
        for (size_t h = 0; h < NUM_HOLDINGS; ++h) {
            const bool has = (holdings[h][0] == needed || holdings[h][1] == needed);
            u += dist[h] * (has ? 1.0 : -1.0);
        }
        return u;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Simple accessors
// ─────────────────────────────────────────────────────────────────────────────

int GameState::get_current_player() const { return current_player; }

void               GameState::set_rules_config(const RulesConfig& rc) { rules_config = rc; }
const RulesConfig& GameState::get_rules_config() const                 { return rules_config; }

void GameState::set_cards(Card p1c1, Card p1c2, Card p2c1, Card p2c2) {
    auto [a, b] = std::minmax(p1c1, p1c2);
    auto [c, d] = std::minmax(p2c1, p2c2);
    p1_cards = {a, b};
    p2_cards = {c, d};
}

void GameState::set_my_cards(std::array<Card, 2> cards) {
    if (current_player == 0) p1_cards = cards;
    else                      p2_cards = cards;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Legal actions — one method per phase
// ─────────────────────────────────────────────────────────────────────────────

void GameState::add_primary_turn_actions(ActionMask& m,
                                          int my_coins, int opp_coins) const {
    m |= action_bit(INCOME);
    m |= action_bit(FOREIGN_AID);
    m |= action_bit(TAX);
    if (opp_coins >= 2)      m |= action_bit(STEAL2);
    else if (opp_coins == 1) m |= action_bit(STEAL1);
    if (my_coins >= COIN_TO_ASSASSINATE) m |= action_bit(ASSASSINATE);
    if (my_coins >= COIN_TO_COUP)        m |= action_bit(COUP);
}

// Phase::ACTION — active player chooses their action.
ActionMask GameState::legal_mask_action() const {
    const bool is_p1   = (current_player == 0);
    const int  my_c    = is_p1 ? p1_coins : p2_coins;
    const int  opp_c   = is_p1 ? p2_coins : p1_coins;

    if (my_c >= COIN_TO_MUST_COUP) return action_bit(COUP);

    ActionMask m = 0;
    add_primary_turn_actions(m, my_c, opp_c);
    return m;
}

// Phase::RESPOND — opponent may allow (take next action), block, or challenge.
//
// "Allow" is implicit: any primary action taken from this phase means the
// opponent is starting their own turn, which records the allow in the pruning
// counters inside advance_phase.  Block and Challenge are explicit.
ActionMask GameState::legal_mask_respond() const {
    const bool is_p1   = (current_player == 0);
    const int  my_c    = is_p1 ? p1_coins : p2_coins;
    const int  opp_c   = is_p1 ? p2_coins : p1_coins;
    const auto& my_cards  = is_p1 ? p1_cards  : p2_cards;
    const auto& my_inf    = is_p1 ? p1_influence : p2_influence;
    const int   my_lives  = my_inf[0] + my_inf[1];

    ActionMask m = 0;

    switch (pending_action) {

        case FOREIGN_AID:
            if (my_c >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(BLOCK_FOREIGN_AID);
            add_primary_turn_actions(m, my_c, opp_c);
            m |= action_bit(BLOCK_FOREIGN_AID);
            return m;

        case TAX:
            if (my_c >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(CHALLENGE);
            add_primary_turn_actions(m, my_c, opp_c);
            m |= action_bit(CHALLENGE);
            return m;

        case STEAL1:
            add_primary_turn_actions(m, my_c, opp_c);
            m |= action_bit(BLOCK_STEAL1_AMB);
            m |= action_bit(BLOCK_STEAL1_CAP);
            m |= action_bit(CHALLENGE);
            return m;

        case STEAL2:
            if (my_c >= COIN_TO_MUST_COUP) return action_bit(COUP) | action_bit(CHALLENGE);
            add_primary_turn_actions(m, my_c, opp_c);
            m |= action_bit(BLOCK_STEAL2_AMB);
            m |= action_bit(BLOCK_STEAL2_CAP);
            m |= action_bit(CHALLENGE);
            return m;

        case ASSASSINATE:
            // Forced: if only one life left, or holding a live Contessa, must
            // block or challenge (can't just let an assassination through).
            if (my_lives == 1) return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
            {
                const bool has_contessa =
                    (my_cards[0] == CONTESSA && my_inf[0] > 0) ||
                    (my_cards[1] == CONTESSA && my_inf[1] > 0);
                if (has_contessa) return action_bit(BLOCK_ASSASSINATE) | action_bit(CHALLENGE);
            }
            m |= action_bit(BLOCK_ASSASSINATE);
            m |= action_bit(CHALLENGE);
            set_card_losing_bits(my_cards, my_inf, m);
            return m;

        default:
            assert(false && "RESPOND phase with non-respondable pending_action");
            return 0;
    }
}

// Phase::CHALLENGE_BLOCK — active player may challenge the block or pass it.
ActionMask GameState::legal_mask_challenge_block() const {
    return action_bit(CHALLENGE) | action_bit(PASS_BLOCK);
}

// Phase::SHOW_CARD — challenged player reveals the card they claimed (or has none).
// If they have the card, they show it (which means the challenger loses an
// influence → LOSE_CARD for the *challenger*).
// If they don't, they go directly to LOSE_CARD themselves.
// The legal mask here is just the SHOW_* action if they have it; the
// code in advance_phase handles the "no card" case as a LOSE_CARD transition.
ActionMask GameState::legal_mask_show_card() const {
    const Card needed       = card_claimed_by(pending_action);
    const bool  is_p1       = (current_player == 0);
    const auto& my_cards    = is_p1 ? p1_cards    : p2_cards;
    const auto& my_inf      = is_p1 ? p1_influence : p2_influence;

    // Check if they hold the needed card.
    for (int i = 0; i < 2; ++i) {
        if (my_inf[i] == 0) continue;
        if (my_cards[i] == needed) {
            switch (needed) {
                case ASSASSIN:  return action_bit(SHOW_ASSASSIN);
                case AMBASSADOR:return action_bit(SHOW_AMBASSADOR);
                case CAPTAIN:   return action_bit(SHOW_CAPTAIN);
                case CONTESSA:  return action_bit(SHOW_CONTESSA);
                case DUKE:      return action_bit(SHOW_DUKE);
            }
        }
    }

    // No card — they must admit it: immediately go to LOSE_CARD.
    // But we still need a legal action to drive the CFR step.
    // We use the LOSE_* actions for the current player's live cards.
    // Special case: if pending is BLOCK_ASSASSINATE and they have 2 lives → LOSE_BOTH.
    const int my_lives = my_inf[0] + my_inf[1];
    if (pending_action == BLOCK_ASSASSINATE && my_lives == 2) {
        return action_bit(LOSE_BOTH);
    }
    ActionMask m = 0;
    set_card_losing_bits(my_cards, my_inf, m);
    return m;
}

// Phase::LOSE_CARD — the losing player chooses which influence to discard.
ActionMask GameState::legal_mask_lose_card() const {
    const bool is_p1     = (current_player == 0);
    const auto& my_cards = is_p1 ? p1_cards    : p2_cards;
    const auto& my_inf   = is_p1 ? p1_influence : p2_influence;

    // COUP or ASSASSINATE that resolved normally (no show) → choose card.
    // The LOSE_BOTH case from a successful ASSASSINATE after SHOW_ASSASSIN
    // is also possible, but is handled by SHOW_CARD → LOSE_CARD transition.
    const int my_lives = my_inf[0] + my_inf[1];
    if (pending_action == ASSASSINATE && my_lives == 2 && false) {
        // (LOSE_BOTH was already returned by legal_mask_show_card if applicable)
    }
    ActionMask m = 0;
    set_card_losing_bits(my_cards, my_inf, m);
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  get_legal_actions — dispatch by phase, then apply pruning
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Action> GameState::get_legal_actions() const {
    if (phase == Phase::TERMINAL) return {};

    ActionMask m = 0;
    switch (phase) {
        case Phase::ACTION:          m = legal_mask_action();          break;
        case Phase::RESPOND:         m = legal_mask_respond();         break;
        case Phase::CHALLENGE_BLOCK: m = legal_mask_challenge_block(); break;
        case Phase::SHOW_CARD:       m = legal_mask_show_card();       break;
        case Phase::LOSE_CARD:       m = legal_mask_lose_card();       break;
        default: break;
    }

    m = apply_pruning_heuristics(m);
    m = add_extension_actions(m);
    return actions_from_mask(m);
}

std::vector<Action> GameState::actions_from_mask(ActionMask m) const {
    std::vector<Action> v;
    for (unsigned a = 0; a < static_cast<unsigned>(NUM_ACTIONS); ++a)
        if (m & action_bit(static_cast<Action>(a)))
            v.push_back(static_cast<Action>(a));
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pruning helpers
// ─────────────────────────────────────────────────────────────────────────────

bool GameState::is_free_turn() const { return phase == Phase::ACTION; }

bool GameState::has_allowed_foreign_aid() const {
    return (current_player == 0) ? (num_p1_has_allowed_foreign_aid > 0)
                                 : (num_p2_has_allowed_foreign_aid > 0);
}
bool GameState::has_allowed_steal() const {
    return (current_player == 0) ? (num_p1_has_allowed_steal > 0)
                                 : (num_p2_has_allowed_steal > 0);
}
bool GameState::has_allowed_assassinate() const {
    return (current_player == 0) ? (num_p1_has_allowed_assassinate > 0)
                                 : (num_p2_has_allowed_assassinate > 0);
}
bool GameState::has_opponent_allowed_tax() const {
    return (current_player == 0) ? (num_p2_has_allowed_tax > 0)
                                 : (num_p1_has_allowed_tax > 0);
}
bool GameState::has_opponent_allowed_foreign_aid() const {
    return (current_player == 0) ? (num_p2_has_allowed_foreign_aid > 0)
                                 : (num_p1_has_allowed_foreign_aid > 0);
}
bool GameState::has_opponent_allowed_steal() const {
    return (current_player == 0) ? (num_p2_has_allowed_steal > 0)
                                 : (num_p1_has_allowed_steal > 0);
}
bool GameState::has_opponent_claimed_duke_2v2(bool is_p1) const {
    return is_p1 ? (num_p2_has_claimed_duke > 0) : (num_p1_has_claimed_duke > 0);
}
bool GameState::has_opponent_claimed_steal_blocker_2v2(bool is_p1) const {
    return is_p1 ? (num_p2_has_claimed_steal_blocker > 0) : (num_p1_has_claimed_steal_blocker > 0);
}
bool GameState::has_opponent_claimed_contessa_2v2(bool is_p1) const {
    return is_p1 ? (num_p2_has_claimed_contessa > 0) : (num_p1_has_claimed_contessa > 0);
}
bool GameState::has_opponent_claimed_duke_xv1(bool is_p1) const {
    if (is_p1) return p2_claims_duke_at_first_loss >= 0 && num_p2_has_claimed_duke > p2_claims_duke_at_first_loss;
    else        return p1_claims_duke_at_first_loss >= 0 && num_p1_has_claimed_duke > p1_claims_duke_at_first_loss;
}
bool GameState::has_opponent_claimed_steal_blocker_xv1(bool is_p1) const {
    if (is_p1) return p2_claims_steal_blocker_at_first_loss >= 0 && num_p2_has_claimed_steal_blocker > p2_claims_steal_blocker_at_first_loss;
    else        return p1_claims_steal_blocker_at_first_loss >= 0 && num_p1_has_claimed_steal_blocker > p1_claims_steal_blocker_at_first_loss;
}
bool GameState::has_opponent_claimed_contessa_xv1(bool is_p1) const {
    if (is_p1) return p2_claims_contessa_at_first_loss >= 0 && num_p2_has_claimed_contessa > p2_claims_contessa_at_first_loss;
    else        return p1_claims_contessa_at_first_loss >= 0 && num_p1_has_claimed_contessa > p1_claims_contessa_at_first_loss;
}

bool GameState::can_force_coup_vs_one_influence() const {
    if (phase != Phase::ACTION) return false;
    const bool is_p1   = (current_player == 0);
    const int  my_c    = is_p1 ? p1_coins : p2_coins;
    const auto& opp_inf = is_p1 ? p2_influence : p1_influence;
    return my_c >= COIN_TO_COUP && (opp_inf[0] + opp_inf[1]) == 1;
}

bool GameState::can_force_claim_mate() const {
    if (!rules_config.extensions.claim_mate_enabled || phase != Phase::ACTION) return false;
    const bool is_p1    = (current_player == 0);
    const int  my_c     = is_p1 ? p1_coins    : p2_coins;
    const int  opp_c    = is_p1 ? p2_coins    : p1_coins;
    const auto& my_cards = is_p1 ? p1_cards   : p2_cards;
    const auto& my_inf   = is_p1 ? p1_influence : p2_influence;
    const auto& opp_inf  = is_p1 ? p2_influence : p1_influence;
    return (my_inf[0]+my_inf[1]) == 2 && (opp_inf[0]+opp_inf[1]) == 1
           && can_2v1_coupmate(my_c, opp_c, my_cards);
}

ActionMask GameState::apply_pruning_heuristics(ActionMask m) const {
    if (!rules_config.pruning.enabled || m == 0) return m;

    const bool is_p1   = (current_player == 0);
    const int  my_c    = is_p1 ? p1_coins    : p2_coins;
    const auto& my_inf   = is_p1 ? p1_influence : p2_influence;
    const auto& opp_inf  = is_p1 ? p2_influence : p1_influence;
    const int my_lives  = my_inf[0]  + my_inf[1];
    const int opp_lives = opp_inf[0] + opp_inf[1];

    // Rule 1: must Coup if ≥7 coins vs 1-influence opponent.
    if (can_force_coup_vs_one_influence()) return action_bit(COUP);

    // Rule 2 (CLAIM_MATE dominates all else when can_force).
    if (can_force_claim_mate())            return action_bit(CLAIM_MATE);

    // Phase-specific pruning only in RESPOND.
    if (phase == Phase::RESPOND) {
        switch (pending_action) {
            case FOREIGN_AID:
                m &= ~action_bit(TAX);
                if (has_allowed_foreign_aid()) m &= ~action_bit(BLOCK_FOREIGN_AID);
                break;
            case TAX:
                m &= ~action_bit(FOREIGN_AID);
                break;
            case STEAL1:
                m &= ~action_bit(INCOME);
                m &= ~action_bit(FOREIGN_AID);
                m &= ~action_bit(STEAL1);
                m &= ~action_bit(STEAL2);
                m &= ~action_bit(ASSASSINATE);
                m &= ~action_bit(COUP);
                if (has_allowed_steal()) {
                    m &= ~action_bit(BLOCK_STEAL1_AMB);
                    m &= ~action_bit(BLOCK_STEAL1_CAP);
                }
                break;
            case STEAL2:
                m &= ~action_bit(INCOME);
                m &= ~action_bit(FOREIGN_AID);
                m &= ~action_bit(STEAL1);
                m &= ~action_bit(STEAL2);
                if (my_c >= COIN_TO_MUST_COUP) m &= ~action_bit(ASSASSINATE);
                if (has_allowed_steal()) {
                    m &= ~action_bit(BLOCK_STEAL2_AMB);
                    m &= ~action_bit(BLOCK_STEAL2_CAP);
                }
                break;
            case ASSASSINATE:
                if (has_allowed_assassinate()) m &= ~action_bit(BLOCK_ASSASSINATE);
                break;
            default: break;
        }
    }

    // 2v2 claim-based pruning (ACTION phase).
    if (phase == Phase::ACTION && my_lives == 2 && opp_lives == 2) {
        if (has_opponent_claimed_steal_blocker_2v2(is_p1)) {
            m &= ~action_bit(STEAL1);
            m &= ~action_bit(STEAL2);
        }
        if (has_opponent_claimed_duke_2v2(is_p1))     m &= ~action_bit(FOREIGN_AID);
        if (has_opponent_claimed_contessa_2v2(is_p1)) m &= ~action_bit(ASSASSINATE);
    }

    // xv1 claim-based pruning (ACTION phase).
    if (phase == Phase::ACTION && opp_lives == 1) {
        if (has_opponent_claimed_steal_blocker_xv1(is_p1)) {
            m &= ~action_bit(STEAL1);
            m &= ~action_bit(STEAL2);
        }
        if (has_opponent_claimed_duke_xv1(is_p1))     m &= ~action_bit(FOREIGN_AID);
        if (has_opponent_claimed_contessa_xv1(is_p1)) m &= ~action_bit(ASSASSINATE);
    }

    // Dominated weaker actions (2v2, ACTION phase).
    if (phase == Phase::ACTION && my_lives == 2 && opp_lives == 2) {
        if (has_opponent_allowed_tax() && (m & action_bit(TAX))) {
            m &= ~action_bit(FOREIGN_AID);
            m &= ~action_bit(INCOME);
        } else if (has_opponent_allowed_steal() && (m & action_bit(STEAL2))) {
            m &= ~action_bit(FOREIGN_AID);
            m &= ~action_bit(INCOME);
        } else if (has_opponent_allowed_steal() && (m & action_bit(STEAL1))) {
            m &= ~action_bit(INCOME);
        } else if (has_opponent_allowed_foreign_aid() && (m & action_bit(FOREIGN_AID))) {
            m &= ~action_bit(INCOME);
        }
    }

    return m;
}

ActionMask GameState::add_extension_actions(ActionMask m) const {
    if (rules_config.extensions.claim_mate_enabled && can_force_claim_mate())
        m |= action_bit(CLAIM_MATE);
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  can_2v1_coupmate  (unchanged logic)
// ─────────────────────────────────────────────────────────────────────────────

bool GameState::can_2v1_coupmate(int my_c, int opp_c,
                                  const std::array<Card, 2>& my_cards) const {
    if (my_cards[1] == DUKE) {
        if (my_c >= 4) return true;
        if (my_c == 3 && opp_c <= 5) return true;
        if (my_c == 2 && opp_c <= 3) return true;
        if (my_c == 1 && opp_c <= 1) return true;
    }
    if (my_cards[0] == CONTESSA && my_cards[1] == DUKE) {
        if (my_c == 3 && opp_c <= 9) return true;
        if (my_c == 2 && opp_c <= 7) return true;
        if (my_c == 1 && opp_c <= 5) return true;
        if (opp_c <= 3) return true;
    }
    if ((my_cards[0] == CAPTAIN && my_cards[1] == DUKE) ||
        (my_cards[0] == AMBASSADOR && my_cards[1] == DUKE)) {
        if (opp_c <= 2) return true;
    }
    if (my_cards[0] == CAPTAIN || my_cards[1] == CAPTAIN ||
        my_cards[0] == AMBASSADOR || my_cards[1] == AMBASSADOR) {
        if (my_c >= 6) return true;
        if (my_c == 5 && opp_c <= 5) return true;
        if (my_c == 4 && opp_c <= 2) return true;
    }
    if ((my_cards[0] == CAPTAIN && my_cards[1] == CONTESSA) ||
        (my_cards[0] == AMBASSADOR && my_cards[1] == CONTESSA)) {
        if (my_c == 5 && opp_c <= 9) return true;
        if (my_c == 4 && opp_c <= 6) return true;
        if (my_c == 3 && opp_c <= 3) return true;
        if (my_c == 2 && opp_c == 0) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Card-lose helpers
// ─────────────────────────────────────────────────────────────────────────────

void GameState::set_card_losing_bits(const std::array<Card, 2>& cards,
                                      const std::array<int, 2>& inf,
                                      ActionMask& m) const {
    auto bit = [](Card c) -> ActionMask {
        switch (c) {
            case ASSASSIN:  return action_bit(LOSE_ASSASSIN);
            case AMBASSADOR:return action_bit(LOSE_AMBASSADOR);
            case CAPTAIN:   return action_bit(LOSE_CAPTAIN);
            case CONTESSA:  return action_bit(LOSE_CONTESSA);
            case DUKE:      return action_bit(LOSE_DUKE);
        }
        assert(false); return 0;
    };
    if (inf[0] == 1) m |= bit(cards[0]);
    if (inf[1] == 1) {
        if (cards[0] != cards[1] || inf[0] == 0)
            m |= bit(cards[1]);
    }
}

std::vector<Action> GameState::get_card_losing_actions(const std::array<Card, 2>& cards,
                                                         const std::array<int, 2>& inf) const {
    ActionMask m = 0;
    set_card_losing_bits(cards, inf, m);
    return actions_from_mask(m);
}

// ─────────────────────────────────────────────────────────────────────────────
//  lose_card / undo_lose_card
//  These handle coin reversal for failed challenges and the pruning snapshots.
// ─────────────────────────────────────────────────────────────────────────────

void GameState::lose_card(Card card) {
    const bool is_p1 = (current_player == 0);
    auto& my_inf   = is_p1 ? p1_influence : p2_influence;
    const auto& my_cards = is_p1 ? p1_cards : p2_cards;
    int& my_c  = is_p1 ? p1_coins : p2_coins;
    int& opp_c = is_p1 ? p2_coins : p1_coins;

    if      (my_cards[0] == card && my_inf[0] != 0) my_inf[0] = 0;
    else if (my_cards[1] == card && my_inf[1] != 0) my_inf[1] = 0;
    else assert(false && "lose_card: card not found");

    // Capture pruning snapshot when first influence is lost.
    if (my_inf[0] + my_inf[1] == 1) {
        if (is_p1) {
            p1_claims_duke_at_first_loss          = num_p1_has_claimed_duke;
            p1_claims_steal_blocker_at_first_loss = num_p1_has_claimed_steal_blocker;
            p1_claims_contessa_at_first_loss      = num_p1_has_claimed_contessa;
        } else {
            p2_claims_duke_at_first_loss          = num_p2_has_claimed_duke;
            p2_claims_steal_blocker_at_first_loss = num_p2_has_claimed_steal_blocker;
            p2_claims_contessa_at_first_loss      = num_p2_has_claimed_contessa;
        }
    }

    // Coin reversal for challenge resolution:
    // pending_action is the action that was challenged.
    // When the challenged player has no card and must lose one, their
    // action's coin effect is reversed.
    if (phase == Phase::SHOW_CARD) {
        // Current player is the one showing (or failing to show).
        // They were the ones who played pending_action.
        switch (pending_action) {
            case TAX:               my_c -= 3; break;
            case STEAL1:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:  my_c -= 1; opp_c += 1; break;
            case STEAL2:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:  my_c -= 2; opp_c += 2; break;
            case ASSASSINATE:       my_c += 3; break;
            case BLOCK_FOREIGN_AID: opp_c += 2; break;
            default: break;
        }
    }
}

void GameState::undo_lose_card(Card card) {
    const bool is_p1 = (current_player == 0);
    auto& my_inf   = is_p1 ? p1_influence : p2_influence;
    const auto& my_cards = is_p1 ? p1_cards : p2_cards;
    int& my_c  = is_p1 ? p1_coins : p2_coins;
    int& opp_c = is_p1 ? p2_coins : p1_coins;

    if      (my_cards[0] == card && my_inf[0] == 0) my_inf[0] = 1;
    else if (my_cards[1] == card && my_inf[1] == 0) my_inf[1] = 1;
    else assert(false && "undo_lose_card: card not found");

    if (my_inf[0] + my_inf[1] == 2) {
        if (is_p1) {
            p1_claims_duke_at_first_loss          = -1;
            p1_claims_steal_blocker_at_first_loss = -1;
            p1_claims_contessa_at_first_loss      = -1;
        } else {
            p2_claims_duke_at_first_loss          = -1;
            p2_claims_steal_blocker_at_first_loss = -1;
            p2_claims_contessa_at_first_loss      = -1;
        }
    }

    if (phase == Phase::SHOW_CARD) {
        switch (pending_action) {
            case TAX:               my_c += 3; break;
            case STEAL1:
            case BLOCK_STEAL1_AMB:
            case BLOCK_STEAL1_CAP:  my_c += 1; opp_c -= 1; break;
            case STEAL2:
            case BLOCK_STEAL2_AMB:
            case BLOCK_STEAL2_CAP:  my_c += 2; opp_c -= 2; break;
            case ASSASSINATE:       my_c -= 3; break;
            case BLOCK_FOREIGN_AID: opp_c -= 2; break;
            default: break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  apply_action_effects / undo_action_effects
//  Pure resource mutation; no phase or player changes.
// ─────────────────────────────────────────────────────────────────────────────

void GameState::apply_action_effects(Action a) {
    const bool is_p1 = (current_player == 0);
    int& my_c  = is_p1 ? p1_coins : p2_coins;
    int& opp_c = is_p1 ? p2_coins : p1_coins;
    auto& my_inf = is_p1 ? p1_influence : p2_influence;

    switch (a) {
        case INCOME:         my_c += 1; break;
        case FOREIGN_AID:    my_c += 2; break;
        case TAX:            my_c += 3; break;
        case STEAL1:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP: my_c += 1; opp_c -= 1; break;
        case STEAL2:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP: my_c += 2; opp_c -= 2; break;
        case ASSASSINATE:    my_c -= COIN_TO_ASSASSINATE; break;
        case COUP:           my_c -= COIN_TO_COUP;        break;
        case BLOCK_FOREIGN_AID: opp_c -= 2; break;
        case LOSE_ASSASSIN:  lose_card(ASSASSIN);  break;
        case LOSE_AMBASSADOR:lose_card(AMBASSADOR); break;
        case LOSE_CAPTAIN:   lose_card(CAPTAIN);   break;
        case LOSE_CONTESSA:  lose_card(CONTESSA);  break;
        case LOSE_DUKE:      lose_card(DUKE);      break;
        case LOSE_BOTH:      my_inf[0] = 0; my_inf[1] = 0; break;
        default: break; // CHALLENGE, PASS_BLOCK, SHOW_*, CLAIM_MATE: no coin/influence effect
    }
}

void GameState::undo_action_effects(Action a) {
    const bool is_p1 = (current_player == 0);
    int& my_c  = is_p1 ? p1_coins : p2_coins;
    int& opp_c = is_p1 ? p2_coins : p1_coins;
    auto& my_inf = is_p1 ? p1_influence : p2_influence;

    switch (a) {
        case INCOME:         my_c -= 1; break;
        case FOREIGN_AID:    my_c -= 2; break;
        case TAX:            my_c -= 3; break;
        case STEAL1:
        case BLOCK_STEAL1_AMB:
        case BLOCK_STEAL1_CAP: my_c -= 1; opp_c += 1; break;
        case STEAL2:
        case BLOCK_STEAL2_AMB:
        case BLOCK_STEAL2_CAP: my_c -= 2; opp_c += 2; break;
        case ASSASSINATE:    my_c += COIN_TO_ASSASSINATE; break;
        case COUP:           my_c += COIN_TO_COUP;        break;
        case BLOCK_FOREIGN_AID: opp_c += 2; break;
        case LOSE_ASSASSIN:  undo_lose_card(ASSASSIN);  break;
        case LOSE_AMBASSADOR:undo_lose_card(AMBASSADOR); break;
        case LOSE_CAPTAIN:   undo_lose_card(CAPTAIN);   break;
        case LOSE_CONTESSA:  undo_lose_card(CONTESSA);  break;
        case LOSE_DUKE:      undo_lose_card(DUKE);      break;
        case LOSE_BOTH:      my_inf[0] = 1; my_inf[1] = 1; break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pruning tracker updates
// ─────────────────────────────────────────────────────────────────────────────

void GameState::update_claim_trackers_on_action(Action a) {
    const bool is_p1 = (current_player == 0);
    switch (a) {
        case TAX:
        case BLOCK_FOREIGN_AID:
            if (is_p1) num_p1_has_claimed_duke++;          else num_p2_has_claimed_duke++;          break;
        case STEAL1: case STEAL2:
        case BLOCK_STEAL1_AMB: case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_AMB: case BLOCK_STEAL2_CAP:
            if (is_p1) num_p1_has_claimed_steal_blocker++; else num_p2_has_claimed_steal_blocker++; break;
        case BLOCK_ASSASSINATE:
            if (is_p1) num_p1_has_claimed_contessa++;      else num_p2_has_claimed_contessa++;      break;
        default: break;
    }
    // A challenge invalidates the pending claim.
    if (a == CHALLENGE) {
        const bool opp_is_p1 = !is_p1;
        switch (pending_action) {
            case TAX: case BLOCK_FOREIGN_AID:
                if (opp_is_p1) num_p1_has_claimed_duke--;          else num_p2_has_claimed_duke--;          break;
            case STEAL1: case STEAL2:
            case BLOCK_STEAL1_AMB: case BLOCK_STEAL1_CAP:
            case BLOCK_STEAL2_AMB: case BLOCK_STEAL2_CAP:
                if (opp_is_p1) num_p1_has_claimed_steal_blocker--; else num_p2_has_claimed_steal_blocker--; break;
            case BLOCK_ASSASSINATE:
                if (opp_is_p1) num_p1_has_claimed_contessa--;      else num_p2_has_claimed_contessa--;      break;
            default: break;
        }
    }
}

void GameState::undo_claim_trackers_on_action(Action a) {
    const bool is_p1 = (current_player == 0);
    switch (a) {
        case TAX: case BLOCK_FOREIGN_AID:
            if (is_p1) num_p1_has_claimed_duke--;          else num_p2_has_claimed_duke--;          break;
        case STEAL1: case STEAL2:
        case BLOCK_STEAL1_AMB: case BLOCK_STEAL1_CAP:
        case BLOCK_STEAL2_AMB: case BLOCK_STEAL2_CAP:
            if (is_p1) num_p1_has_claimed_steal_blocker--; else num_p2_has_claimed_steal_blocker--; break;
        case BLOCK_ASSASSINATE:
            if (is_p1) num_p1_has_claimed_contessa--;      else num_p2_has_claimed_contessa--;      break;
        default: break;
    }
    if (a == CHALLENGE) {
        const bool opp_is_p1 = !is_p1;
        switch (pending_action) {
            case TAX: case BLOCK_FOREIGN_AID:
                if (opp_is_p1) num_p1_has_claimed_duke++;          else num_p2_has_claimed_duke++;          break;
            case STEAL1: case STEAL2:
            case BLOCK_STEAL1_AMB: case BLOCK_STEAL1_CAP:
            case BLOCK_STEAL2_AMB: case BLOCK_STEAL2_CAP:
                if (opp_is_p1) num_p1_has_claimed_steal_blocker++; else num_p2_has_claimed_steal_blocker++; break;
            case BLOCK_ASSASSINATE:
                if (opp_is_p1) num_p1_has_claimed_contessa++;      else num_p2_has_claimed_contessa++;      break;
            default: break;
        }
    }
}

// Allow trackers fire when the responding player takes an action that is NOT
// a block/challenge — meaning they accepted the pending action.
// advance_phase calls this just before transitioning out of RESPOND.
void GameState::update_allow_trackers_on_action(Action a) {
    // current_player is still the responder at the point this is called.
    const bool responder_is_p1 = (current_player == 0);
    if (phase != Phase::RESPOND) return;

    const bool is_challenge = (a == CHALLENGE);
    const bool is_block     = ::is_block(a);

    if (!is_challenge && !is_block) {
        // Took a primary action = allowed the pending action.
        switch (pending_action) {
            case FOREIGN_AID:
                if (responder_is_p1) num_p1_has_allowed_foreign_aid++; else num_p2_has_allowed_foreign_aid++; break;
            case STEAL1: case STEAL2:
                if (responder_is_p1) num_p1_has_allowed_steal++;        else num_p2_has_allowed_steal++;       break;
            case ASSASSINATE:
                if (responder_is_p1) num_p1_has_allowed_assassinate++;  else num_p2_has_allowed_assassinate++; break;
            default: break;
        }
    }
    if (a == PASS_BLOCK || (!is_challenge && !is_block && pending_action == TAX)) {
        // Passing a block or not challenging TAX = allowed TAX.
        if (pending_action == TAX) {
            if (responder_is_p1) num_p1_has_allowed_tax++; else num_p2_has_allowed_tax++;
        }
    }
}

void GameState::undo_allow_trackers_on_action(Action a) {
    // Mirrors update_allow_trackers_on_action.
    // current_player is the responder (restored before this is called).
    const bool responder_is_p1 = (current_player == 0);
    if (phase != Phase::RESPOND) return;

    const bool is_challenge = (a == CHALLENGE);
    const bool is_block     = ::is_block(a);

    if (!is_challenge && !is_block) {
        switch (pending_action) {
            case FOREIGN_AID:
                if (responder_is_p1) num_p1_has_allowed_foreign_aid--; else num_p2_has_allowed_foreign_aid--; break;
            case STEAL1: case STEAL2:
                if (responder_is_p1) num_p1_has_allowed_steal--;        else num_p2_has_allowed_steal--;       break;
            case ASSASSINATE:
                if (responder_is_p1) num_p1_has_allowed_assassinate--;  else num_p2_has_allowed_assassinate--; break;
            default: break;
        }
    }
    if (a == PASS_BLOCK || (!is_challenge && !is_block && pending_action == TAX)) {
        if (pending_action == TAX) {
            if (responder_is_p1) num_p1_has_allowed_tax--; else num_p2_has_allowed_tax--;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phase transition table
//
//  advance_phase(a) is called AFTER apply_action_effects, with current_player
//  still set to whoever just acted.  It updates phase, active_player,
//  current_player, and pending_action.
//
//  The table below is the source of truth for every transition.
//
//  From ACTION:
//    INCOME / any primary that is unchallenged/unblocked: if it's INCOME or
//    a non-challengeable action, go to ACTION with flipped player.
//    Challengeable / blockable primaries: go to RESPOND, opponent acts.
//
//  From RESPOND (pending = primary action):
//    Primary action taken (allow): RESPOND → ACTION, swap active+current player.
//    CHALLENGE:                    RESPOND → SHOW_CARD, opponent (claimer) shows.
//    BLOCK_*:                      RESPOND → CHALLENGE_BLOCK, original active player responds.
//
//  From CHALLENGE_BLOCK (pending = block):
//    PASS_BLOCK:    CHALLENGE_BLOCK → ACTION, swap (blocker succeeded, turn ends).
//    CHALLENGE:     CHALLENGE_BLOCK → SHOW_CARD, blocker shows their card.
//
//  From SHOW_CARD:
//    SHOW_* (has card):  challenger loses card → LOSE_CARD for challenger.
//    LOSE_* (no card):   claimer loses card directly (lose_card already applied).
//                        Depending on context, may also need to resolve the
//                        original action's effect (e.g. successful ASSASSINATE).
//
//  From LOSE_CARD:
//    LOSE_*:  → ACTION (next turn) or TERMINAL.
// ─────────────────────────────────────────────────────────────────────────────

// Stored between advance_phase and retreat_phase so undo_action can reverse
// the transition exactly.  We store them as a small struct pushed onto a vector.
struct PhaseSnapshot {
    Phase  phase;
    int    active_player;
    int    current_player;
    Action pending_action;
};

// We keep a stack of snapshots parallel to history.
// (A static vector is fine since GameState is not thread-shared during CFR.)
// To avoid polluting the class header further we use a file-level variable.
// NOTE: this is intentionally non-member; one per process.
static std::vector<PhaseSnapshot> phase_stack;

void GameState::advance_phase(Action a) {
    // Save current phase state so undo_action can restore it exactly.
    phase_stack.push_back({phase, active_player, current_player, pending_action});

    // ── Handle allow-tracker updates while we still know we're in RESPOND ──
    update_allow_trackers_on_action(a);

    const int actor = current_player;  // who just acted
    const int other = 1 - actor;

    switch (phase) {

        case Phase::ACTION: {
            // CLAIM_MATE / COUP / INCOME go straight to TERMINAL or next ACTION.
            if (a == CLAIM_MATE) {
                phase = Phase::TERMINAL;
                return;
            }
            if (a == COUP) {
                // Opponent must choose which card to lose.
                phase          = Phase::LOSE_CARD;
                pending_action = COUP;
                current_player = other;
                return;
            }
            if (a == INCOME) {
                // Unchallenged / unblockable: next turn.
                phase          = Phase::ACTION;
                active_player  = other;
                current_player = other;
                return;
            }
            // All other primary actions can be responded to.
            phase          = Phase::RESPOND;
            pending_action = a;
            current_player = other; // opponent responds
            return;
        }

        case Phase::RESPOND: {
            if (a == CHALLENGE) {
                // Opponent challenged pending_action.  Claimer (actor) shows.
                phase          = Phase::SHOW_CARD;
                current_player = other; // other = the original actor who made the claim
                return;
            }
            if (is_block(a)) {
                // Opponent blocks.  active_player may challenge the block.
                phase          = Phase::CHALLENGE_BLOCK;
                pending_action = a; // now pending is the block itself
                current_player = other; // other = original active player
                return;
            }
            // Responder took a primary action → implicit allow.
            // pending_action's effect already applied.  Now it's responder's turn.
            phase          = Phase::RESPOND;
            pending_action = a;
            active_player  = actor;    // responder becomes the new active player
            current_player = other;    // original actor now responds
            return;
        }

        case Phase::CHALLENGE_BLOCK: {
            if (a == PASS_BLOCK) {
                // Active player concedes; blocker succeeded.  Turn ends.
                phase          = Phase::ACTION;
                active_player  = other;
                current_player = other;
                return;
            }
            // Active player challenges the block.
            assert(a == CHALLENGE);
            phase          = Phase::SHOW_CARD;
            current_player = other; // other = the blocker who must show
            return;
        }

        case Phase::SHOW_CARD: {
            if (a >= SHOW_ASSASSIN && a <= SHOW_DUKE) {
                // Successful show: challenger loses a card.
                // The challenger is 1-current_player (the one who issued the challenge).
                phase          = Phase::LOSE_CARD;
                current_player = other; // challenger must now lose a card
                return;
            }
            // No card (LOSE_* or LOSE_BOTH played from SHOW_CARD):
            // The claimer just lost their card (effect already applied by apply_action_effects).
            // If the original claim was ASSASSINATE and it resolved, the target
            // still needs to lose a card — but that was already their LOSE_CARD from
            // the RESPOND phase (ASSASSINATE goes: ACTION→RESPOND→[no challenge]→LOSE_CARD).
            // Here in SHOW_CARD the claimer failed, so the action just fizzles.
            // Move to next turn.
            phase          = Phase::ACTION;
            active_player  = other;
            current_player = other;
            // Check terminal.
            if ((p1_influence[0]+p1_influence[1] == 0) || (p2_influence[0]+p2_influence[1] == 0))
                phase = Phase::TERMINAL;
            return;
        }

        case Phase::LOSE_CARD: {
            // Player lost a card.  Check terminal, else next turn.
            if ((p1_influence[0]+p1_influence[1] == 0) || (p2_influence[0]+p2_influence[1] == 0)) {
                phase = Phase::TERMINAL;
                return;
            }
            // Whose turn is next?  It depends on what triggered the LOSE_CARD.
            // pending_action at this point is:
            //   COUP        → active_player couped, so next turn = opponent of couper = actor
            //   ASSASSINATE → active_player assassinated, so next turn = active_player advances
            //   SHOW_*      → challenger lost card, so next turn = active_player (the original actor)
            //   Primary won by challenge → next = original active_player
            // In all cases the player who did NOT just lose a card goes next,
            // which is other = 1 - actor.  active_player was set before LOSE_CARD.
            phase          = Phase::ACTION;
            current_player = active_player; // whoever held the turn
            return;
        }

        default:
            assert(false && "advance_phase called in unexpected phase");
    }
}

void GameState::retreat_phase(Action /*a*/) {
    assert(!phase_stack.empty());
    const PhaseSnapshot& snap = phase_stack.back();
    phase          = snap.phase;
    active_player  = snap.active_player;
    current_player = snap.current_player;
    pending_action = snap.pending_action;
    phase_stack.pop_back();
}

// ─────────────────────────────────────────────────────────────────────────────
//  do_action / undo_action
// ─────────────────────────────────────────────────────────────────────────────

void GameState::do_action(Action a) {
    history.push_back(a);

    update_claim_trackers_on_action(a);   // uses current phase + pending_action
    apply_action_effects(a);              // coins, influence (LOSE_* calls lose_card)
    advance_phase(a);                     // updates phase/active/current/pending
    // After advance_phase, check terminal due to LOSE_BOTH.
    if (!is_terminal() &&
        (p1_influence[0]+p1_influence[1] == 0 || p2_influence[0]+p2_influence[1] == 0))
        phase = Phase::TERMINAL;
}

void GameState::undo_action() {
    assert(!history.empty());
    const Action a = history.back();

    retreat_phase(a);                     // restores phase/active/current/pending
    undo_action_effects(a);               // reverses coins, influence
    undo_allow_trackers_on_action(a);     // NOTE: called after retreat so phase is restored
    undo_claim_trackers_on_action(a);

    history.pop_back();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hashing
// ─────────────────────────────────────────────────────────────────────────────

size_t GameState::get_history_hash(const std::vector<Action>& hist) {
    size_t h = 14695981039346656037ULL;
    constexpr size_t P = 1099511628211ULL;
    h ^= 10; h *= P;
    h ^= 10; h *= P;
    for (Action a : hist) { h ^= static_cast<size_t>(a); h *= P; }
    return h;
}

size_t GameState::get_history_hash() const { return get_history_hash(history); }

size_t GameState::get_infoset_hash() const {
    const auto& my_cards = (current_player == 0) ? p1_cards : p2_cards;
    size_t h = 14695981039346656037ULL;
    constexpr size_t P = 1099511628211ULL;
    h ^= static_cast<size_t>(my_cards[0]); h *= P;
    h ^= static_cast<size_t>(my_cards[1]); h *= P;
    for (Action a : history) { h ^= static_cast<size_t>(a); h *= P; }
    return h;
}

std::string GameState::get_infoset_string() const {
    static const char* const CARD_NAMES[] = {
        "ASSASSIN", "AMBASSADOR", "CAPTAIN", "CONTESSA", "DUKE"
    };
    const auto& my_cards = (current_player == 0) ? p1_cards : p2_cards;
    std::string s;
    s.reserve(60 + history.size() * 16);
    s += CARD_NAMES[my_cards[0]];
    s += ' ';
    s += CARD_NAMES[my_cards[1]];
    s += ": ";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i) s += ", ";
        s += ACTION_NAMES[history[i]];
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Debugging
// ─────────────────────────────────────────────────────────────────────────────

static const char* phase_name(Phase p) {
    switch (p) {
        case Phase::ACTION:          return "ACTION";
        case Phase::RESPOND:         return "RESPOND";
        case Phase::CHALLENGE_BLOCK: return "CHALLENGE_BLOCK";
        case Phase::SHOW_CARD:       return "SHOW_CARD";
        case Phase::LOSE_CARD:       return "LOSE_CARD";
        case Phase::TERMINAL:        return "TERMINAL";
    }
    return "?";
}

void GameState::print_history() const {
    std::cout << "History: ";
    for (Action a : history) std::cout << ACTION_NAMES[a] << ' ';
    std::cout << '\n';
}

void GameState::print_game_state() const { std::cout << get_game_state(); }

std::string GameState::get_game_state() const {
    std::string o;
    o += "Phase: ";           o += phase_name(phase);         o += '\n';
    o += "Active player: ";   o += std::to_string(active_player);  o += '\n';
    o += "Current player: ";  o += std::to_string(current_player); o += '\n';
    o += "Pending action: ";  o += ACTION_NAMES[pending_action];   o += '\n';

    static const char* const CN[] = {"ASSASSIN","AMBASSADOR","CAPTAIN","CONTESSA","DUKE"};
    o += "P1 Cards: ";
    o += CN[p1_cards[0]]; o += (p1_influence[0] ? "(A) " : "(D) ");
    o += CN[p1_cards[1]]; o += (p1_influence[1] ? "(A)" : "(D)"); o += '\n';
    o += "P2 Cards: ";
    o += CN[p2_cards[0]]; o += (p2_influence[0] ? "(A) " : "(D) ");
    o += CN[p2_cards[1]]; o += (p2_influence[1] ? "(A)" : "(D)"); o += '\n';
    o += "P1 Coins: "; o += std::to_string(p1_coins); o += '\n';
    o += "P2 Coins: "; o += std::to_string(p2_coins); o += '\n';

    o += "History: ";
    for (Action a : history) { o += ACTION_NAMES[a]; o += ' '; }
    o += '\n';

    o += "Legal actions: ";
    for (Action a : get_legal_actions()) { o += ACTION_NAMES[a]; o += ' '; }
    o += "\n\n";
    return o;
}