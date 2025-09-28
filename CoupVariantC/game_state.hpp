#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <vector>

enum Action {
  INCOME,
  TAX,
  ASSASSINATE,
  COUP,
  BLOCK_ASSASSINATE,
  CHALLENGE,
  PASS_BLOCK,
};

enum Card {ASSASSIN, CONTESSA, DUKE};

class GameState {
public:
    int current_player;
    Card p1_card;
    Card p2_card;
    int p1_coins;
    int p2_coins;
    bool p1_did_assassinate;
    bool p2_did_assassinate;
    std::vector<Action> history;

public:
    GameState();
    bool is_terminal() const;
    double get_utility() const;
    int get_current_player() const;
    void set_cards(Card, Card);
    std::vector<Action> get_legal_actions() const;
    void apply_action(Action);
    void undo_action();
    size_t get_hash();
    std::string get_infoset_string() const;
};

#endif