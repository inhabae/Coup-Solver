#ifndef COUP_VARIANT_A_STATE_HPP
#define COUP_VARIANT_A_STATE_HPP

#include <memory>
#include <sstream>
#include <string>
#include <vector>

using Player = int;

enum Action {
  GAME_START,
  INCOME,
  ASSASSINATE,
  COUP,
  BLOCK_ASSASSINATE,
  CHALLENGE,
  PASS_BLOCK,
  LOSE_CARD,
  SHOW_CARD
};

enum Card { INVALID_CARD, ASSASSIN, CONTESSA, CIVILIAN};

class CoupVariantAState {
public:
  Player current_player;
  Card player1_card;
  Card player2_card;
  int player1_coins;
  int player2_coins;
  bool did_assassinate_p1;
  bool did_assassinate_p2;
  std::vector<Action> action_history;

  CoupVariantAState();
  bool is_terminal() const;
  bool is_chance_node() const;
  std::vector<double> get_utilities() const;
  Player get_current_player() const;
  std::vector<Action> get_legal_actions() const;
  std::unique_ptr<CoupVariantAState> get_child(Action action) const;
  std::string get_information_set_key() const;
};

#endif // COUP_VARIANT_A_STATE_HPP