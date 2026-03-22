#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

namespace {

bool contains_action(const std::vector<Action>& actions, Action target) {
    return std::find(actions.begin(), actions.end(), target) != actions.end();
}

void assert_contains(const std::vector<Action>& actions, Action target) {
    assert(contains_action(actions, target));
}

void assert_not_contains(const std::vector<Action>& actions, Action target) {
    assert(!contains_action(actions, target));
}

void test_baseline_mode_removes_pruning_only_restrictions() {
    GameState baseline(RulesConfig::baseline_default());
    baseline.do_action(FOREIGN_AID);
    const std::vector<Action> actions = baseline.get_legal_actions();

    assert_contains(actions, TAX);
    assert_contains(actions, BLOCK_FOREIGN_AID);
    assert_not_contains(actions, CLAIM_MATE);
}

void test_solver_mode_keeps_existing_foreign_aid_pruning() {
    GameState solver(RulesConfig::solver_default());
    solver.do_action(FOREIGN_AID);
    const std::vector<Action> actions = solver.get_legal_actions();

    assert_not_contains(actions, TAX);
    assert_contains(actions, BLOCK_FOREIGN_AID);
}

void test_solver_mode_disallows_assassinate_and_coup_after_steal1() {
    GameState solver(RulesConfig::solver_default());
    solver.p2_coins = 1;
    solver.do_action(STEAL1);

    const std::vector<Action> actions = solver.get_legal_actions();
    assert_contains(actions, TAX);
    assert_contains(actions, CHALLENGE);
    assert_not_contains(actions, ASSASSINATE);
    assert_not_contains(actions, COUP);
}

void test_baseline_mode_disallows_losing_last_influence_to_assassinate() {
    GameState baseline(RulesConfig::baseline_default());
    baseline.set_cards(CONTESSA, DUKE, ASSASSIN, ASSASSIN);
    baseline.p1_influence = {1, 0};
    baseline.p1_coins = 2;
    baseline.current_player = 0;
    baseline.history = {ASSASSINATE};

    const std::vector<Action> actions = baseline.get_legal_actions();
    assert_contains(actions, BLOCK_ASSASSINATE);
    assert_contains(actions, CHALLENGE);
    assert_not_contains(actions, LOSE_CONTESSA);
}

void test_baseline_mode_disallows_losing_live_contessa_to_assassinate() {
    GameState baseline(RulesConfig::baseline_default());
    baseline.set_cards(CONTESSA, DUKE, ASSASSIN, ASSASSIN);
    baseline.p1_influence = {1, 1};
    baseline.current_player = 0;
    baseline.history = {ASSASSINATE};

    const std::vector<Action> actions = baseline.get_legal_actions();
    assert_contains(actions, BLOCK_ASSASSINATE);
    assert_contains(actions, CHALLENGE);
    assert_not_contains(actions, LOSE_CONTESSA);
    assert_not_contains(actions, LOSE_DUKE);
}

void test_solver_mode_prunes_assassination_loss_option() {
    GameState solver(RulesConfig::solver_default());
    solver.set_cards(CONTESSA, DUKE, ASSASSIN, ASSASSIN);
    solver.p1_influence = {1, 0};
    solver.p1_coins = 2;
    solver.current_player = 0;
    solver.history = {ASSASSINATE};

    const std::vector<Action> actions = solver.get_legal_actions();
    assert_contains(actions, BLOCK_ASSASSINATE);
    assert_contains(actions, CHALLENGE);
    assert_not_contains(actions, LOSE_CONTESSA);
}

void test_claim_mate_is_extension_only() {
    RulesConfig extension_only = RulesConfig::baseline_default();
    extension_only.extensions.claim_mate_enabled = true;

    GameState extension_state(extension_only);
    extension_state.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    extension_state.p1_coins = 4;
    extension_state.p2_coins = 0;
    extension_state.p2_influence = {1, 0};

    const std::vector<Action> extension_actions = extension_state.get_legal_actions();
    assert_contains(extension_actions, CLAIM_MATE);
    assert_contains(extension_actions, TAX);
    assert_not_contains(extension_actions, COUP);

    GameState solver(RulesConfig::solver_default());
    solver.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    solver.p1_coins = 4;
    solver.p2_coins = 0;
    solver.p2_influence = {1, 0};

    const std::vector<Action> solver_actions = solver.get_legal_actions();
    assert(solver_actions.size() == 1);
    assert_contains(solver_actions, CLAIM_MATE);
}

void test_claim_mate_do_undo_symmetry() {
    RulesConfig extension_only = RulesConfig::baseline_default();
    extension_only.extensions.claim_mate_enabled = true;

    GameState state(extension_only);
    state.set_cards(DUKE, DUKE, ASSASSIN, ASSASSIN);
    state.p1_coins = 4;
    state.p2_coins = 0;
    state.p2_influence = {1, 0};

    const std::string before = state.get_game_state();
    state.do_action(CLAIM_MATE);
    assert(state.is_terminal());
    state.undo_action();
    assert(state.get_game_state() == before);
}

} // namespace

int main() {
    test_baseline_mode_removes_pruning_only_restrictions();
    test_solver_mode_keeps_existing_foreign_aid_pruning();
    test_solver_mode_disallows_assassinate_and_coup_after_steal1();
    test_baseline_mode_disallows_losing_last_influence_to_assassinate();
    test_baseline_mode_disallows_losing_live_contessa_to_assassinate();
    test_solver_mode_prunes_assassination_loss_option();
    test_claim_mate_is_extension_only();
    test_claim_mate_do_undo_symmetry();

    std::cout << "All tests passed\n";
    return 0;
}
