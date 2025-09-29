#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <cstddef>
#include <string>
#include <vector>

enum Action {
  INCOME,
  TAX,
  STEAL1,
  STEAL2,
  ASSASSINATE,
  COUP,
  BLOCK_STEAL1,
  BLOCK_STEAL2,
  BLOCK_ASSASSINATE,
  CHALLENGE,
  PASS_BLOCK,
};

enum Card {ASSASSIN, CAPTAIN, CONTESSA, DUKE};

class GameState {
public:
    int current_player;
    Card p1_card;
    Card p2_card;
    int p1_coins;
    int p2_coins;
    bool p1_did_assassinate;
    bool p2_did_assassinate;
    int p1_num_stolen;
    int p2_num_stolen;
    int p1_num_steal_blocked;
    int p2_num_steal_blocked;
    std::vector<Action> history;

public:
    GameState();
    bool is_terminal() const;
    double get_utility() const;
    double get_br_utility(int, std::vector<double>) const;
    int get_current_player() const;
    void set_cards(Card, Card);
    void set_my_card(Card);
    std::vector<Action> get_legal_actions() const;
    void apply_action(Action);
    void undo_action();
    size_t get_hash();
    std::string get_infoset_string() const;
};

#endif