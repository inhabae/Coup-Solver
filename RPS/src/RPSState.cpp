#include "RPSState.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

RPSState::RPSState() {
  current_player = 0;
  player1_card = INVALID_CARD;
  player2_card = INVALID_CARD;
  action_history = {};
}

bool RPSState::is_terminal() const {
  if (action_history.size() > 3) {
    std::cerr << "[ERROR] Invalid action_history size: "
              << action_history.size() << std::endl;
    std::exit(1);
  }
  return action_history.size() == 3;
}

bool RPSState::is_chance_node() const { return action_history.size() == 0; }

Player RPSState::get_current_player() const { return current_player; }

std::vector<Action> RPSState::get_legal_actions() const {
  return {ROCK, PAPER, SCISSORS};
}

std::unique_ptr<RPSState> RPSState::get_child(Action action) const {
  auto new_state = std::make_unique<RPSState>(*this);
  if (action == GAME_START) {
    return new_state;
  }
  new_state->current_player = 1 - current_player;
  new_state->action_history.push_back(action);
  return new_state;
}

std::string RPSState::get_information_set_key() const {
  static const std::unordered_map<Action, std::string> action_to_string = {
      {GAME_START, "GAME_START"},
      {ROCK, "ROCK"},
      {PAPER, "PAPER"},
      {SCISSORS, "SCISSORS"}};

  std::stringstream ss;
  for (size_t i = 0; i < action_history.size(); ++i) {
    if (i == 0)
      ss << "GAME_START ";
    else
      ss << "HIDDEN_ACTION ";
  }

  std::unordered_map<Card, std::string> card_to_string = {
      {CARD1, "C1"}, {CARD2, "C2"}, {CARD3, "C3"}};

  if (current_player == 0) {
    if (player1_card == INVALID_CARD) {
      std::cerr << "[ERROR] Invalid player1_card for P1" << std::endl;
      std::exit(1);
    }
    return "P1:" + card_to_string.at(player1_card) + ":" + ss.str();
  }
  if (current_player == 1) {
    if (player2_card == INVALID_CARD) {
      std::cerr << "[ERROR] Invalid player2_card for P2" << std::endl;
      std::exit(1);
    }
    return "P2:" + card_to_string.at(player2_card) + ":" + ss.str();
  }

  std::cerr << "[ERROR] get_information_set_key() does not return."
            << std::endl;
  std::exit(1);
}

std::vector<double> RPSState::get_utilities() const {
  std::vector<double> utils(2, 0.0);
  if (!is_terminal()) {
    std::cerr << "[ERROR] get_utilities() called on a non-terminal node"
              << std::endl;
    std::exit(1);
  }

  if (action_history[1] == action_history[2])
    return {0.0, 0.0};
  if ((action_history[1] == ROCK && action_history[2] == SCISSORS) ||
      (action_history[1] == SCISSORS && action_history[2] == PAPER) ||
      (action_history[1] == PAPER && action_history[2] == ROCK)) {
    return {1.0, -1.0};
  }
  return {-1.0, 1.0};
}