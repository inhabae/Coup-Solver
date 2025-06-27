#ifndef RPS_STATE_HPP
#define RPS_STATE_HPP

#include <memory>
#include <sstream>
#include <string>
#include <vector>

using Player = int;

enum Action {
  GAME_START,
  ROCK,
  PAPER,
  SCISSORS,
};

enum Card { INVALID_CARD, CARD1, CARD2, CARD3 };

class RPSState {
public:
  Player current_player;
  Card player1_card;
  Card player2_card;
  std::vector<Action> action_history;

  RPSState();
  bool is_terminal() const;
  bool is_chance_node() const;
  std::vector<double> get_utilities() const;
  Player get_current_player() const;
  std::vector<Action> get_legal_actions() const;
  std::unique_ptr<RPSState> get_child(Action action) const;
  std::string get_information_set_key() const;
};

#endif // RPS_STATE_HPP