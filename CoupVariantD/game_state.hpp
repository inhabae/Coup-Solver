#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <array>
#include <cstddef>
#include <string>
#include <vector>

enum Action {
  INCOME = 0,
  FOREIGN_AID = 1,
  TAX = 2,
  STEAL1 = 3,
  STEAL2 = 4,
  ASSASSINATE = 5,
  COUP = 6,
  BLOCK_FOREIGN_AID = 7,
  BLOCK_STEAL1_AMB = 8,
  BLOCK_STEAL2_AMB = 9,
  BLOCK_STEAL1_CAP = 10,
  BLOCK_STEAL2_CAP = 11,
  BLOCK_ASSASSINATE = 12,
  CHALLENGE = 13,
  PASS_BLOCK = 14,
  SHOW_ASSASSIN = 15,
  SHOW_AMBASSADOR = 16,
  SHOW_CAPTAIN = 17,
  SHOW_CONTESSA = 18,
  SHOW_DUKE = 19,
  LOSE_ASSASSIN = 20,
  LOSE_AMBASSADOR = 21,
  LOSE_CAPTAIN = 22,
  LOSE_CONTESSA = 23,
  LOSE_DUKE = 24,
  LOSE_BOTH = 25, // Double assassination
};

enum Card { ASSASSIN, AMBASSADOR, CAPTAIN, CONTESSA, DUKE };

const int COIN_TO_ASSASSINATE = 3;
const int COIN_TO_COUP = 7;
const int COIN_TO_MUST_COUP = 10;
const int MAX_BLOCK_NUM = 2;

class GameState {
public:
  int current_player;
  std::array<Card, 2> p1_cards;
  std::array<Card, 2> p2_cards;
  std::array<int, 2> p1_influence;
  std::array<int, 2> p2_influence;
  int p1_coins;
  int p2_coins;
  int p1_num_assassinate_blocked;
  int p2_num_assassinate_blocked;
  int p1_num_steal_blocked;
  int p2_num_steal_blocked;
  int p1_num_fa_blocked;
  int p2_num_fa_blocked;
  std::vector<Action> history;

public:
  GameState();
  bool is_terminal() const;
  double get_utility() const;
  double get_br_utility(int, std::vector<double>) const;
  int get_current_player() const;
  void set_cards(Card, Card, Card, Card);
  void set_my_cards(const std::array<Card, 2>);
  std::vector<Action> get_legal_actions() const;
  std::vector<Action> get_card_losing_actions(const std::array<Card, 2>, const std::array<int, 2>) const;
  void lose_card(Card);
  void undo_lose_card(Card);
  void apply_action(Action);
  void undo_action();
  size_t get_hash() const;
  std::string get_infoset_string() const;
  void print_game_state() const;
  std::string get_game_state() const;
};

#endif