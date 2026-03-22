#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

namespace {

bool has(const std::vector<Action>& v, Action a) {
    return std::find(v.begin(), v.end(), a) != v.end();
}
void must_have(const std::vector<Action>& v, Action a) { assert(has(v, a)); }
void must_not(const std::vector<Action>& v, Action a)  { assert(!has(v, a)); }

// ── Helper: advance state through a sequence of actions ──────────────────────
void seq(GameState& g, std::initializer_list<Action> actions) {
    for (Action a : actions) g.do_action(a);
}

// ── 1. Starting phase is ACTION, player 0 ────────────────────────────────────
void test_initial_state() {
    GameState g(RulesConfig::baseline_default());
    assert(g.phase == Phase::ACTION);
    assert(g.active_player  == 0);
    assert(g.current_player == 0);
    assert(!g.is_terminal());
}

// ── 2. INCOME: ACTION → ACTION, player flips ─────────────────────────────────
void test_income_flips_turn() {
    GameState g(RulesConfig::baseline_default());
    g.do_action(INCOME);
    assert(g.phase == Phase::ACTION);
    assert(g.active_player  == 1);
    assert(g.current_player == 1);
    assert(g.p1_coins == 3);
}

// ── 3. FOREIGN_AID → RESPOND, opponent can block or allow ────────────────────
void test_foreign_aid_enters_respond() {
    GameState g(RulesConfig::baseline_default());
    g.do_action(FOREIGN_AID);
    assert(g.phase == Phase::RESPOND);
    assert(g.pending_action == FOREIGN_AID);
    assert(g.current_player == 1);  // opponent responds
    auto acts = g.get_legal_actions();
    must_have(acts, BLOCK_FOREIGN_AID);
    must_have(acts, INCOME);   // allow by taking own turn action
    must_not(acts, TAX);       // pruned: no TAX after FA in solver mode (baseline has it)
}

void test_foreign_aid_allow_legal_in_baseline() {
    GameState g(RulesConfig::baseline_default());
    g.do_action(FOREIGN_AID);
    auto acts = g.get_legal_actions();
    must_have(acts, TAX);  // baseline: no pruning, TAX is legal
}

// ── 4. TAX → RESPOND; CHALLENGE → SHOW_CARD ──────────────────────────────────
void test_tax_challenge_show_card_phase() {
    GameState g(RulesConfig::baseline_default());
    g.set_cards(DUKE, ASSASSIN, CAPTAIN, CONTESSA);
    // P0 plays TAX
    g.do_action(TAX);
    assert(g.phase == Phase::RESPOND);
    assert(g.pending_action == TAX);
    // P1 challenges
    g.do_action(CHALLENGE);
    assert(g.phase == Phase::SHOW_CARD);
    // P0 holds Duke, so must show it
    auto acts = g.get_legal_actions();
    assert(acts.size() == 1);
    assert(acts[0] == SHOW_DUKE);
}

// ── 5. Successful show: challenger (P1) loses a card ─────────────────────────
void test_successful_show_sends_challenger_to_lose_card() {
    GameState g(RulesConfig::baseline_default());
    g.set_cards(DUKE, ASSASSIN, CAPTAIN, CONTESSA);
    seq(g, {TAX, CHALLENGE, SHOW_DUKE});
    // Now P1 (challenger) must lose a card
    assert(g.phase == Phase::LOSE_CARD);
    assert(g.current_player == 1);
    auto acts = g.get_legal_actions();
    must_have(acts, LOSE_CAPTAIN);
    must_have(acts, LOSE_CONTESSA);
}

// ── 6. Failed show: claimer loses a card, action fizzles ─────────────────────
void test_failed_show_claimer_loses_card() {
    GameState g(RulesConfig::baseline_default());
    // P0 has ASSASSIN+CAPTAIN, claims TAX (bluff)
    g.set_cards(ASSASSIN, CAPTAIN, CONTESSA, DUKE);
    g.do_action(TAX);    // P0 claims Duke (bluff)
    g.do_action(CHALLENGE);  // P1 challenges
    // P0 is in SHOW_CARD but has no Duke → must lose a card directly
    assert(g.phase == Phase::SHOW_CARD);
    auto acts = g.get_legal_actions();
    must_have(acts, LOSE_ASSASSIN);
    must_have(acts, LOSE_CAPTAIN);
    must_not(acts, SHOW_DUKE);
    // P0 loses ASSASSIN
    g.do_action(LOSE_ASSASSIN);
    // Action fizzles; no TAX coins given (they were given, then reversed inside lose_card).
    assert(g.p1_coins == 2);  // 2 start + 3 tax - 3 reversal = 2
    // Turn passes to P1
    assert(g.phase == Phase::ACTION);
    assert(g.current_player == 1);
}

// ── 7. BLOCK → CHALLENGE_BLOCK → PASS_BLOCK: turn ends ──────────────────────
void test_block_pass_ends_turn() {
    GameState g(RulesConfig::baseline_default());
    g.do_action(FOREIGN_AID);   // P0 claims FA
    g.do_action(BLOCK_FOREIGN_AID); // P1 blocks
    assert(g.phase == Phase::CHALLENGE_BLOCK);
    assert(g.current_player == 0); // P0 may challenge block or pass
    g.do_action(PASS_BLOCK);
    // Block succeeded, FA cancelled; P1's turn
    assert(g.phase == Phase::ACTION);
    assert(g.current_player == 1);
    assert(g.p1_coins == 2); // FA coins not gained
}

// ── 8. BLOCK → CHALLENGE → challenger loses (blocker had card) ───────────────
void test_block_challenge_blocker_wins() {
    GameState g(RulesConfig::baseline_default());
    // P1 has Duke, so block FA is legitimate
    g.set_cards(ASSASSIN, ASSASSIN, DUKE, CONTESSA);
    g.do_action(FOREIGN_AID);          // P0 claims FA
    g.do_action(BLOCK_FOREIGN_AID);    // P1 blocks (claims Duke)
    g.do_action(CHALLENGE);            // P0 challenges block
    assert(g.phase == Phase::SHOW_CARD);
    assert(g.current_player == 1);     // P1 (blocker) must show
    auto acts = g.get_legal_actions();
    assert(acts.size() == 1 && acts[0] == SHOW_DUKE);
    g.do_action(SHOW_DUKE);
    assert(g.phase == Phase::LOSE_CARD);
    assert(g.current_player == 0);     // P0 (challenger) loses a card
}

// ── 9. COUP → LOSE_CARD ──────────────────────────────────────────────────────
void test_coup_sends_to_lose_card() {
    GameState g(RulesConfig::baseline_default());
    g.p1_coins = 7;
    g.do_action(COUP);
    assert(g.phase == Phase::LOSE_CARD);
    assert(g.current_player == 1);
    assert(g.p1_coins == 0);
    auto acts = g.get_legal_actions();
    must_have(acts, LOSE_ASSASSIN);
}

// ── 10. ASSASSINATE → RESPOND → lose card directly ───────────────────────────
void test_assassinate_respond_lose_card() {
    GameState g(RulesConfig::baseline_default());
    g.p1_coins = 3;
    // P1 has two lives, no Contessa, no challenge instinct
    g.set_cards(ASSASSIN, CAPTAIN, DUKE, AMBASSADOR);
    g.do_action(ASSASSINATE);
    assert(g.phase == Phase::RESPOND);
    assert(g.pending_action == ASSASSINATE);
    assert(g.current_player == 1);
    // P1 accepts (takes a turn action = allow) — but in baseline mode they can
    // also lose a card directly without blocking.
    auto acts = g.get_legal_actions();
    must_have(acts, LOSE_DUKE);
    must_have(acts, LOSE_AMBASSADOR);
    must_have(acts, BLOCK_ASSASSINATE);
    must_have(acts, CHALLENGE);
    // P1 loses a card directly
    g.do_action(LOSE_DUKE);
    assert(g.phase == Phase::ACTION);
    assert(g.current_player == 1); // P1's turn now (they responded)
}

// ── 11. do/undo symmetry ─────────────────────────────────────────────────────
void test_do_undo_symmetry() {
    GameState g(RulesConfig::baseline_default());
    g.set_cards(DUKE, ASSASSIN, CAPTAIN, CONTESSA);

    // Snapshot before
    const std::string before = g.get_game_state();

    // Play a full challenge sequence and undo it all
    g.do_action(TAX);
    g.do_action(CHALLENGE);
    g.do_action(SHOW_DUKE);
    g.do_action(LOSE_CAPTAIN);

    g.undo_action(); // LOSE_CAPTAIN
    g.undo_action(); // SHOW_DUKE
    g.undo_action(); // CHALLENGE
    g.undo_action(); // TAX

    assert(g.get_game_state() == before);
}

// ── 12. do/undo symmetry for COUP ────────────────────────────────────────────
void test_do_undo_coup() {
    GameState g(RulesConfig::baseline_default());
    g.p1_coins = 7;
    const std::string before = g.get_game_state();
    g.do_action(COUP);
    g.do_action(LOSE_ASSASSIN);
    g.undo_action();
    g.undo_action();
    assert(g.get_game_state() == before);
}

// ── 13. Terminal via influence loss ──────────────────────────────────────────
void test_terminal_via_influence_loss() {
    GameState g(RulesConfig::baseline_default());
    g.p1_coins = 7;
    g.p2_influence = {1, 0}; // P2 has one life
    g.do_action(COUP);
    g.do_action(LOSE_ASSASSIN);
    assert(g.is_terminal());
    // Utility: current_player after LOSE_ASSASSIN flip points to… P0 (won)
    // get_utility returns from current_player's perspective
    assert(g.get_utility() == 1.0 || g.get_utility() == -1.0); // just confirm it's set
}

// ── 14. Baseline: no pruning (TAX allowed after FOREIGN_AID response) ────────
void test_baseline_mode_removes_pruning_only_restrictions() {
    GameState g(RulesConfig::baseline_default());
    g.do_action(FOREIGN_AID);
    auto acts = g.get_legal_actions();
    must_have(acts, TAX);
    must_have(acts, BLOCK_FOREIGN_AID);
    must_not(acts, CLAIM_MATE);
}

// ── 15. Solver: TAX pruned after FOREIGN_AID response ────────────────────────
void test_solver_mode_keeps_existing_foreign_aid_pruning() {
    GameState g(RulesConfig::solver_default());
    g.do_action(FOREIGN_AID);
    auto acts = g.get_legal_actions();
    must_not(acts, TAX);
    must_have(acts, BLOCK_FOREIGN_AID);
}

// ── 16. Solver: after STEAL1 the responder cannot take other primary actions ──
void test_solver_mode_disallows_assassinate_and_coup_after_steal1() {
    GameState g(RulesConfig::solver_default());
    g.p2_coins = 1;
    g.do_action(STEAL1);
    auto acts = g.get_legal_actions();
    must_have(acts, TAX);
    must_have(acts, CHALLENGE);
    must_not(acts, ASSASSINATE);
    must_not(acts, COUP);
}

// ── 17. Can't lose Contessa under assassination if alive ──────────────────────
void test_baseline_mode_disallows_losing_live_contessa_to_assassinate() {
    GameState g(RulesConfig::baseline_default());
    g.set_cards(CONTESSA, DUKE, ASSASSIN, ASSASSIN);
    g.p1_coins = 2;
    g.p1_influence = {1, 1};
    g.do_action(ASSASSINATE); // P0 assassinates P1
    auto acts = g.get_legal_actions();
    must_have(acts, BLOCK_ASSASSINATE);
    must_have(acts, CHALLENGE);
    must_not(acts, LOSE_CONTESSA);
    must_not(acts, LOSE_DUKE);
}

// ── 18. CLAIM_MATE extension ──────────────────────────────────────────────────
void test_claim_mate_is_extension_only() {
    RulesConfig ext = RulesConfig::baseline_default();
    ext.extensions.claim_mate_enabled = true;

    GameState g(ext);
    g.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    g.p1_coins = 4;
    g.p2_coins = 0;
    g.p2_influence = {1, 0};

    auto acts = g.get_legal_actions();
    must_have(acts, CLAIM_MATE);
    must_have(acts, TAX);       // extension only, no force-prune
    must_not(acts, COUP);       // not 7 coins

    // In solver mode with pruning, CLAIM_MATE dominates everything.
    GameState gs(RulesConfig::solver_default());
    gs.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    gs.p1_coins = 4;
    gs.p2_coins = 0;
    gs.p2_influence = {1, 0};

    auto sacts = gs.get_legal_actions();
    assert(sacts.size() == 1);
    assert(sacts[0] == CLAIM_MATE);
}

// ── 19. CLAIM_MATE do/undo symmetry ──────────────────────────────────────────
void test_claim_mate_do_undo_symmetry() {
    RulesConfig ext = RulesConfig::baseline_default();
    ext.extensions.claim_mate_enabled = true;

    GameState g(ext);
    g.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    g.p1_coins = 4;
    g.p2_coins = 0;
    g.p2_influence = {1, 0};

    const std::string before = g.get_game_state();
    g.do_action(CLAIM_MATE);
    assert(g.is_terminal());
    g.undo_action();
    assert(g.get_game_state() == before);
}

// ── 20. Phase is ACTION at start of every new turn after full exchange ────────
void test_phase_resets_to_action_after_full_sequence() {
    GameState g(RulesConfig::baseline_default());
    g.set_cards(DUKE, ASSASSIN, CAPTAIN, CONTESSA);
    // P0: TAX → P1: challenge → P0: show Duke → P1: lose Captain
    seq(g, {TAX, CHALLENGE, SHOW_DUKE, LOSE_CAPTAIN});
    assert(g.phase == Phase::ACTION);
    assert(g.current_player == 1); // P1's turn (they just lost and now act)
}

} // namespace

int main() {
    test_initial_state();
    test_income_flips_turn();
    test_foreign_aid_enters_respond();
    test_foreign_aid_allow_legal_in_baseline();
    test_tax_challenge_show_card_phase();
    test_successful_show_sends_challenger_to_lose_card();
    test_failed_show_claimer_loses_card();
    test_block_pass_ends_turn();
    test_block_challenge_blocker_wins();
    test_coup_sends_to_lose_card();
    test_assassinate_respond_lose_card();
    test_do_undo_symmetry();
    test_do_undo_coup();
    test_terminal_via_influence_loss();
    test_baseline_mode_removes_pruning_only_restrictions();
    test_solver_mode_keeps_existing_foreign_aid_pruning();
    test_solver_mode_disallows_assassinate_and_coup_after_steal1();
    test_baseline_mode_disallows_losing_live_contessa_to_assassinate();
    test_claim_mate_is_extension_only();
    test_claim_mate_do_undo_symmetry();
    test_phase_resets_to_action_after_full_sequence();

    std::cout << "All tests passed\n";
    return 0;
}