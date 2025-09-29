#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <cstddef>
#include <string>
#include <vector>

enum Action {
  INCOME,
  FOREIGN_AID,
  TAX,
  STEAL1,
  STEAL2,
  ASSASSINATE,
  COUP,
  BLOCK_FOREIGN_AID,
  BLOCK_STEAL1_AMB, // Blocked by Ambassador
  BLOCK_STEAL2_AMB,
  BLOCK_STEAL1_CAP, // Blocked by Captain
  BLOCK_STEAL2_CAP,
  BLOCK_ASSASSINATE,
  CHALLENGE,
  PASS_BLOCK,
};

enum Card {ASSASSIN, AMBASSADOR, CAPTAIN, CONTESSA, DUKE};

const int COIN_TO_ASSASSINATE = 3;
const int COIN_TO_COUP = 7;

class GameState {
public:
    int current_player;
    Card p1_card;
    Card p2_card;
    int p1_coins;
    int p2_coins;
    int p1_num_assassinate_blocked;
    int p2_num_assassinate_blocked;
    int p1_num_steal_blocked;
    int p2_num_steal_blocked;
    int p1_num_fa_blocked; // Number of blocked foreign aid
    int p2_num_fa_blocked;
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