#include "game_state.hpp"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <cassert>

void test1() {
    // This is Game 1 vs Derek
    GameState g;
    std::vector<Action> expected = {};
    g.set_cards(ASSASSIN, DUKE, ASSASSIN, ASSASSIN);

    // TAX - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // TAX - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // COUP (LOSE ASSASSIN) - COUP (LOSE ASSASSIN)
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, COUP, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(COUP);

    expected = {LOSE_ASSASSIN};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_ASSASSIN);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL1, ASSASSINATE, COUP};
    assert(g.get_legal_actions() == expected);
    g.apply_action(COUP);

    expected = {LOSE_ASSASSIN, LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_ASSASSIN);
    
    // TAX - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL1};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // TAX - ASSASSINATE (CHALLENGE SHOW ASSASSIN LOSE DUKE)
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(ASSASSINATE);

    expected = {BLOCK_ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(CHALLENGE);

    expected = {SHOW_ASSASSIN};
    assert(g.get_legal_actions() == expected);
    g.apply_action(SHOW_ASSASSIN);

    expected = {LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_DUKE);


    expected = {};
    assert(g.get_legal_actions() == expected);

    assert(g.is_terminal());
    assert(g.get_utility() == -1.0);
}

void test2() {
    // This is Game 3 vs Derek
    GameState g;
    std::vector<Action> expected = {};
    g.set_cards(AMBASSADOR, DUKE, ASSASSIN, DUKE);

    // TAX - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // TAX - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // COUP (LOSE DUKE) - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, COUP, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(COUP);

    expected = {LOSE_ASSASSIN, LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_DUKE);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL1, ASSASSINATE, COUP};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // TAX - COUP (LOSE CAPTAIN)
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {COUP};
    assert(g.get_legal_actions() == expected);
    g.apply_action(COUP);

    expected = {LOSE_AMBASSADOR, LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_AMBASSADOR);

    // TAX - ASSASSINATE (CHALLENGE SHOW ASSASSIN LOSE DUKE)
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(ASSASSINATE);

    expected = {BLOCK_ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(CHALLENGE);

    expected = {SHOW_ASSASSIN};
    assert(g.get_legal_actions() == expected);
    g.apply_action(SHOW_ASSASSIN);

    expected = {LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_DUKE);

    assert(g.is_terminal());
    assert(g.get_utility() == -1.0);
}

void test3() {
    // This is a hypothetical game
    GameState g;
    std::vector<Action> expected = {};
    g.set_cards(ASSASSIN, CONTESSA, CAPTAIN, DUKE);

    // INCOME - TAX
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2};
    assert(g.get_legal_actions() == expected);
    g.apply_action(INCOME);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2};
    assert(g.get_legal_actions() == expected);
    g.apply_action(TAX);

    // ASSASSINATE (LOSE DUKE) - FOREIGN_AID
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(ASSASSINATE);

    expected = {BLOCK_ASSASSINATE, CHALLENGE, LOSE_CAPTAIN, LOSE_DUKE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_DUKE);

    expected = {INCOME, FOREIGN_AID, TAX, ASSASSINATE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(FOREIGN_AID);

    // FOREIGN_AID - STEAL2 (BLOCK_STEAL2_AMB CHALLENGE LOSE CONTESSA) 
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, BLOCK_FOREIGN_AID};
    assert(g.get_legal_actions() == expected);
    g.apply_action(FOREIGN_AID);

    expected = {INCOME, FOREIGN_AID, TAX, STEAL2, ASSASSINATE, COUP, BLOCK_FOREIGN_AID};
    assert(g.get_legal_actions() == expected);
    g.apply_action(STEAL2);

    expected = {TAX, BLOCK_STEAL2_AMB, BLOCK_STEAL2_CAP, CHALLENGE};
    assert(g.get_legal_actions() == expected);
    g.apply_action(BLOCK_STEAL2_AMB);

    expected = {CHALLENGE, PASS_BLOCK};
    assert(g.get_legal_actions() == expected);
    g.apply_action(CHALLENGE);

    expected = {LOSE_ASSASSIN, LOSE_CONTESSA};
    assert(g.get_legal_actions() == expected);
    g.apply_action(LOSE_CONTESSA);

    // STEAL2 COUP 
    // Game-winning COUP is forced
    expected = {INCOME, FOREIGN_AID, TAX, STEAL2};
    assert(g.get_legal_actions() == expected);
    g.apply_action(STEAL2);

    expected = {COUP};
    assert(g.get_legal_actions() == expected);
    g.apply_action(COUP);

    g.apply_action(LOSE_ASSASSIN);

    assert(g.is_terminal());
    assert(g.get_utility() == -1.0);
}

void test4() {
    // Testing whether game-winning Coup is correctly forced
    GameState g;
    std::vector<Action> expected;
    g.set_cards(ASSASSIN, ASSASSIN, CONTESSA, CONTESSA);
    g.p1_influence = {0, 1};
    
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);

    expected = {COUP};
    assert(g.get_legal_actions() == expected);
}

void test5() {
    // Game 8 vs Derek
    GameState g;
    g.set_cards(CONTESSA, DUKE, CAPTAIN, DUKE);
    
    g.print_game_state();
    g.apply_action(TAX);

    g.print_game_state();
    g.apply_action(TAX);

    g.print_game_state();
    g.apply_action(TAX);

    g.print_game_state();
    g.apply_action(TAX);

    g.print_game_state();
    g.apply_action(COUP);

    g.print_game_state();
    g.apply_action(LOSE_DUKE);

    g.print_game_state();
    g.apply_action(COUP);

    g.print_game_state();
    g.apply_action(LOSE_DUKE);

    g.print_game_state();
    g.apply_action(FOREIGN_AID);

    g.print_game_state();
    g.apply_action(FOREIGN_AID);

    g.print_game_state();
    g.apply_action(FOREIGN_AID);

    g.print_game_state();
    g.apply_action(STEAL2);

    g.print_game_state();
    g.apply_action(BLOCK_STEAL2_CAP);

    g.print_game_state();
    g.apply_action(CHALLENGE);

    g.print_game_state();
    g.apply_action(LOSE_CONTESSA);

    g.print_game_state();
}

void test6() {
    // Game 7 vs Derek
    GameState g;
    g.set_cards(ASSASSIN, CAPTAIN, ASSASSIN, DUKE);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(ASSASSINATE);
    g.apply_action(LOSE_CAPTAIN);
    g.apply_action(COUP);
    g.apply_action(LOSE_ASSASSIN);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(ASSASSINATE);
    g.apply_action(CHALLENGE);
    g.apply_action(SHOW_ASSASSIN);
    g.apply_action(LOSE_DUKE);
}

void test7() {
    // Testing correct challenge to nullify the challenged action

    // TAX
    GameState g1;
    g1.set_cards(ASSASSIN, CAPTAIN, ASSASSIN, DUKE);
    g1.apply_action(TAX);
    g1.apply_action(CHALLENGE);
    g1.apply_action(LOSE_ASSASSIN);
    assert(g1.p1_coins == 2);

    // STEAL2
    GameState g2;
    g2.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, DUKE);
    g2.apply_action(STEAL2);
    g2.apply_action(CHALLENGE);
    g2.apply_action(LOSE_ASSASSIN);
    assert(g2.p1_coins == 2);

    // STEAL1
    GameState g3;
    g3.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, DUKE);
    g3.apply_action(STEAL2);
    g3.apply_action(INCOME);
    g3.apply_action(STEAL1);
    g3.apply_action(CHALLENGE);
    g3.apply_action(LOSE_ASSASSIN);
    assert(g3.p1_coins == 4 && g3.p2_coins == 1);

    // BLOCK_FOREIGN_AID
    GameState g4;
    g4.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, CONTESSA);
    g4.apply_action(FOREIGN_AID);
    g4.apply_action(BLOCK_FOREIGN_AID);
    g4.apply_action(CHALLENGE);
    g4.apply_action(LOSE_CONTESSA);
    assert(g4.p1_coins == 4 && g4.p2_coins == 2);

    // BLOCK_STEAL2_CAP WITHOUT CAPTAIN
    GameState g5;
    g5.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    g5.apply_action(STEAL2);
    g5.apply_action(BLOCK_STEAL2_CAP);
    g5.apply_action(CHALLENGE);
    g5.apply_action(LOSE_AMBASSADOR);
    assert(g5.p1_coins == 4 && g5.p2_coins == 0);

    // BLOCK_STEAL_2_CAP WITH CAPTAIN
    GameState g6;
    g6.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, CAPTAIN);
    g6.apply_action(STEAL2);
    g6.apply_action(BLOCK_STEAL2_CAP);
    g6.apply_action(CHALLENGE);
    g6.apply_action(SHOW_CAPTAIN);
    g6.apply_action(LOSE_ASSASSIN);
    assert(g6.p1_coins == 2 && g6.p2_coins == 2);

    // Fake ASSASSINATE should return 3 coins
    GameState g7;
    g7.set_cards(DUKE, DUKE, ASSASSIN, CAPTAIN);
    g7.apply_action(INCOME);
    g7.apply_action(INCOME);
    g7.apply_action(ASSASSINATE);
    g7.apply_action(CHALLENGE);
    g7.apply_action(LOSE_DUKE);
    assert(g7.p1_coins == 3 && g7.p2_coins == 3);
}

void test8() {
    // Testing double assassination
    GameState g1;
    g1.set_cards(ASSASSIN, CAPTAIN, ASSASSIN, DUKE);
    g1.apply_action(INCOME);
    g1.apply_action(INCOME);
    g1.apply_action(ASSASSINATE);
    g1.apply_action(CHALLENGE);
    g1.apply_action(SHOW_ASSASSIN);
    g1.apply_action(LOSE_BOTH);
    assert(g1.is_terminal());

    GameState g2;
    g2.set_cards(ASSASSIN, CAPTAIN, ASSASSIN, DUKE);
    g2.apply_action(INCOME);
    g2.apply_action(INCOME);
    g2.apply_action(ASSASSINATE);
    g2.apply_action(BLOCK_ASSASSINATE);
    g2.apply_action(CHALLENGE);
    g2.apply_action(LOSE_BOTH);
    assert(g2.is_terminal());
}

void test9() {
    // Testing basic undo
    GameState g;
    std::string s1;
    std::string s2;

    // INCOME
    s1 = g.get_game_state();
    g.apply_action(INCOME);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    s1 = g.get_game_state();
    g.apply_action(INCOME);
    g.apply_action(INCOME);
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // FOREIGN_AID
    s1 = g.get_game_state();
    g.apply_action(FOREIGN_AID);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // TAX
    s1 = g.get_game_state();
    g.apply_action(TAX);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // STEAL1
    s1 = g.get_game_state();
    g.apply_action(STEAL2);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // STEAL2
    g.apply_action(STEAL2);
    g.apply_action(INCOME);
    s1 = g.get_game_state();
    g.apply_action(STEAL1);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // ASSASSINATE
    s1 = g.get_game_state();
    g.apply_action(ASSASSINATE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // ASSASSINATE
    s1 = g.get_game_state();
    g.apply_action(ASSASSINATE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);
    g.apply_action(TAX);

    // COUP
    s1 = g.get_game_state();
    g.apply_action(COUP);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);
}

void test10() {
    // Testing basic undo
    GameState g;
    std::string s1;
    std::string s2;

    // BLOCK_FOREIGN_AID
    g.apply_action(FOREIGN_AID);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_FOREIGN_AID);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // BLOCK_STEAL2_AMB
    g.apply_action(STEAL2);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_STEAL2_AMB);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // BLOCK_STEAL2_CAP
    g.apply_action(STEAL2);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_STEAL2_CAP);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.apply_action(STEAL2);
    g.apply_action(INCOME);

    // BLOCK_STEAL1_AMB
    g.apply_action(STEAL1);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_STEAL1_AMB);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // BLOCK_STEAL1_CAP
    g.apply_action(STEAL1);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_STEAL1_CAP);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // BLOCK_ASSASSINATE
    g.apply_action(ASSASSINATE);
    s1 = g.get_game_state();
    g.apply_action(BLOCK_ASSASSINATE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // CHALLENGE
    s1 = g.get_game_state();
    g.apply_action(CHALLENGE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // PASS_BLOCK
    s1 = g.get_game_state();
    g.apply_action(PASS_BLOCK);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // SHOW_ASSASSIN
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(SHOW_ASSASSIN);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // RESET
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();

    g.set_cards(AMBASSADOR, CAPTAIN, CONTESSA, DUKE);

    // SHOW_AMBASSADOR
    g.apply_action(TAX);
    g.apply_action(STEAL2);
    g.apply_action(BLOCK_STEAL2_AMB);
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(SHOW_AMBASSADOR);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.undo_action();
    g.undo_action();

    // SHOW_CAPTAIN
    g.apply_action(BLOCK_STEAL2_CAP);
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(SHOW_CAPTAIN);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.undo_action();
    g.undo_action();
    g.undo_action();

    // SHOW_CONTESSA
    g.apply_action(TAX);
    g.apply_action(ASSASSINATE);
    g.apply_action(BLOCK_ASSASSINATE);
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(SHOW_CONTESSA);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.undo_action();
    g.undo_action();
    g.undo_action();

    // SHOW_DUKE
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(SHOW_DUKE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.undo_action();
    g.undo_action();
    g.undo_action();

    // LOSE_ASSASSIN
    g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, DUKE);
    g.apply_action(TAX);
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(LOSE_ASSASSIN);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    s1 = g.get_game_state();
    g.apply_action(LOSE_ASSASSIN);
    g.apply_action(STEAL2);
    g.apply_action(BLOCK_STEAL2_CAP);
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // LOSE_AMBASSADOR
    g.set_cards(AMBASSADOR, AMBASSADOR, ASSASSIN, DUKE);
    s1 = g.get_game_state();
    g.apply_action(LOSE_AMBASSADOR);
    g.apply_action(STEAL2);
    g.apply_action(BLOCK_STEAL2_CAP);
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // LOSE_CAPTAIN
    g.set_cards(CAPTAIN, CAPTAIN, ASSASSIN, DUKE);
    s1 = g.get_game_state();
    g.apply_action(LOSE_CAPTAIN);
    g.apply_action(STEAL2);
    g.apply_action(BLOCK_STEAL2_CAP);
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // LOSE_CAPTAIN
    g.set_cards(CONTESSA, CONTESSA, ASSASSIN, DUKE);
    s1 = g.get_game_state();
    g.apply_action(LOSE_CONTESSA);
    g.apply_action(STEAL2);
    g.apply_action(BLOCK_STEAL2_CAP);
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    g.undo_action();
    // LOSE_DUKE
    g.set_cards(ASSASSIN, DUKE, ASSASSIN, DUKE);
    g.apply_action(STEAL2);
    g.apply_action(CHALLENGE);
    s1 = g.get_game_state();
    g.apply_action(LOSE_DUKE);
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);
}

void test11() {
    // Double assassination
    GameState g;
    std::string s1;
    std::string s2;

    g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, DUKE);
    g.apply_action(INCOME);
    g.apply_action(INCOME);

    // Challenging ASSASSINATE
    s1 = g.get_game_state();
    g.apply_action(ASSASSINATE);
    g.apply_action(CHALLENGE);
    g.apply_action(SHOW_ASSASSIN);
    g.apply_action(LOSE_BOTH);
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);

    // Challenging BLOCK_ASSASSINATE
    s1 = g.get_game_state();
    g.apply_action(ASSASSINATE);
    g.apply_action(BLOCK_ASSASSINATE);
    g.apply_action(CHALLENGE);
    g.apply_action(LOSE_BOTH);
    assert(g.is_terminal());
    g.undo_action();
    g.undo_action();
    g.undo_action();
    g.undo_action();
    s2 = g.get_game_state();
    assert(s1 == s2);
}

void test12() {
    // This is Game 3 vs Derek
    GameState g;
    g.set_cards(AMBASSADOR, DUKE, ASSASSIN, DUKE);

    std::vector<std::string> string_vec;

    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(COUP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_DUKE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(COUP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_AMBASSADOR);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(ASSASSINATE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(CHALLENGE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(SHOW_ASSASSIN);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_DUKE);

    for (int i = 0; i < 15; i++) {
        g.undo_action();
        assert (string_vec.back() == g.get_game_state());
        string_vec.pop_back();
    }
}

void test13() {
    // Game 8 vs Derek
    GameState g;
    g.set_cards(CONTESSA, DUKE, CAPTAIN, DUKE);
    std::vector<std::string> string_vec;
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(COUP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_DUKE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(COUP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_DUKE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(FOREIGN_AID);
    string_vec.push_back(g.get_game_state());
    g.apply_action(FOREIGN_AID);
    string_vec.push_back(g.get_game_state());
    g.apply_action(FOREIGN_AID);
    string_vec.push_back(g.get_game_state());
    g.apply_action(STEAL2);
    string_vec.push_back(g.get_game_state());
    g.apply_action(BLOCK_STEAL2_CAP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(CHALLENGE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_CONTESSA);

    for (int i = 0; i < 15; i++) {
        g.undo_action();
        assert (string_vec.back() == g.get_game_state());
        string_vec.pop_back();
    }
}

void test14() {
    // Hypothetical game from test3()

    GameState g;
    std::vector<std::string> string_vec;
    g.set_cards(ASSASSIN, CONTESSA, CAPTAIN, DUKE);

    string_vec.push_back(g.get_game_state());
    g.apply_action(INCOME);
    string_vec.push_back(g.get_game_state());
    g.apply_action(TAX);
    string_vec.push_back(g.get_game_state());
    g.apply_action(ASSASSINATE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_DUKE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(FOREIGN_AID);
    string_vec.push_back(g.get_game_state());
    g.apply_action(FOREIGN_AID);
    string_vec.push_back(g.get_game_state());
    g.apply_action(STEAL2);
    string_vec.push_back(g.get_game_state());
    g.apply_action(BLOCK_STEAL2_AMB);
    string_vec.push_back(g.get_game_state());
    g.apply_action(CHALLENGE);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_CONTESSA);
    string_vec.push_back(g.get_game_state());
    g.apply_action(STEAL2);
    string_vec.push_back(g.get_game_state());
    g.apply_action(COUP);
    string_vec.push_back(g.get_game_state());
    g.apply_action(LOSE_ASSASSIN);

    for (int i = 0; i < 13; i++) {
        g.undo_action();
        assert (string_vec.back() == g.get_game_state());
        string_vec.pop_back();
    }
}
int main() {
    test1();
    test2();
    test3();
    // test4();
    // test5(); 
    // test6();
    test7();
    test8();
    test9();
    test10();
    test11();
    test12();
    test13();
    test14();
    return 0;
}

/* 

    // COIN CHECK
    std::cout << "P1 coin: " << g.p1_coins << " P2 coin: " << g.p2_coins << std::endl;

    // LEGAL ACTIONS CHECK
    std::cout << "Legal actions: ";
    for (Action a : g.get_legal_actions()) {
        std::cout << a << " ";
    }

*/