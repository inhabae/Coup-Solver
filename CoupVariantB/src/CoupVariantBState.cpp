#include "CoupVariantBState.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

CoupVariantBState::CoupVariantBState() {
  current_player = 0;
  player1_card = INVALID_CARD;
  player2_card = INVALID_CARD;
  player1_coins = 0;
  player2_coins = 0;
  did_assassinate_p1 = false;
  did_assassinate_p2 = false;
  action_history = {};
}

bool CoupVariantBState::is_terminal() const {
  if (action_history.size() == 0) return false;
  return action_history.back() == LOSE_CARD;
}

bool CoupVariantBState::is_chance_node() const {
  return action_history.size() == 0;
}

Player CoupVariantBState::get_current_player() const { return current_player; }

std::vector<Action> CoupVariantBState::get_legal_actions() const {
  Action last_action = action_history.back();
  int current_player_coins = (current_player == 0) ? player1_coins : player2_coins;
    if (last_action == INCOME || last_action == PASS_BLOCK || last_action == GAME_START) {
        if (current_player_coins >= 3) return {COUP};
        if (current_player_coins == 2) {
            if (current_player == 0 && !did_assassinate_p1) {
                return {INCOME, TAX, ASSASSINATE};
            } else if (current_player == 1 && !did_assassinate_p2) {
                return {INCOME, TAX, ASSASSINATE};
            }
            return {INCOME, TAX};
        }
        return {INCOME, TAX};
    } else if (last_action == TAX) {
        if (current_player_coins >= 3) return {COUP};
        if (current_player_coins == 2) {
            if (current_player == 0 && !did_assassinate_p1) {
                return {INCOME, TAX, ASSASSINATE, CHALLENGE};
            } else if (current_player == 1 && !did_assassinate_p2) {
                return {INCOME, TAX, ASSASSINATE, CHALLENGE};
            }
            return {INCOME, TAX, CHALLENGE};
        }
        return {INCOME, TAX, CHALLENGE};
    } else if (last_action == ASSASSINATE) { 
        return {BLOCK_ASSASSINATE, CHALLENGE};
    } else if (last_action == COUP) {
        return {LOSE_CARD};
    } else if (last_action == BLOCK_ASSASSINATE) {
        return {CHALLENGE, PASS_BLOCK};
    } else if (last_action == CHALLENGE) {
        if (current_player == 0) {
            Action challenged_action = action_history[action_history.size() - 2];
            if (challenged_action == ASSASSINATE && player1_card == ASSASSIN) return {SHOW_CARD};
            if (challenged_action == TAX && player1_card == DUKE) return {SHOW_CARD};
            if (challenged_action == BLOCK_ASSASSINATE && player1_card == CONTESSA) return {SHOW_CARD};
            return {LOSE_CARD};
        } else if (current_player == 1) {
            Action challenged_action = action_history[action_history.size() - 2];
            if (challenged_action == ASSASSINATE && player2_card == ASSASSIN) return {SHOW_CARD};
            if (challenged_action == TAX && player2_card == DUKE) return {SHOW_CARD};
            if (challenged_action == BLOCK_ASSASSINATE && player2_card == CONTESSA) return {SHOW_CARD};
            return {LOSE_CARD};
        }
    } else if (last_action == SHOW_CARD) {
        return {LOSE_CARD};
    }

  std::cerr << "[ERROR] get_legal_actions() does not return." << std::endl;
  std::exit(1);
}

std::unique_ptr<CoupVariantBState> CoupVariantBState::get_child(
    Action action) const {
  auto new_state = std::make_unique<CoupVariantBState>(*this);

  if (action == GAME_START) {
        return new_state;
    }
    new_state->current_player = 1 - current_player;
    new_state->action_history.push_back(action);

    if (action == INCOME) {
        if (current_player == 0) new_state->player1_coins = player1_coins + 1;
        if (current_player == 1) new_state->player2_coins = player2_coins + 1;
        return new_state;
    } else if (action == TAX) {
        if (current_player == 0) new_state->player1_coins = player1_coins + 2;
        if (current_player == 1) new_state->player2_coins = player2_coins + 2;
        return new_state;
    } else if (action == ASSASSINATE) {
        if (current_player == 0) {
            new_state->player1_coins = player1_coins - 2;
            new_state->did_assassinate_p1 = true;
        }  else if (current_player == 1) {
            new_state->player2_coins = player2_coins - 2;
            new_state->did_assassinate_p2 = true;
        }
        return new_state;
    } 

  return new_state;
}

std::string CoupVariantBState::get_information_set_key() const {
  std::stringstream ss;
  for (size_t i = 0; i < action_history.size(); ++i) {
    ss << action_history[i] << " ";
  }

  std::unordered_map<Card, std::string> card2string = {
    {ASSASSIN, "ASSASSIN"}, {CONTESSA, "CONTESSA"}, {DUKE, "DUKE"}};

  if (current_player == 0) return "P1:" + card2string[player1_card] + ":" + ss.str();
  if (current_player == 1) return "P2:" + card2string[player2_card] + ":" + ss.str();

  std::cerr << "[ERROR] get_infomratino_set_key() does not return."
            << std::endl;
  std::exit(1);
}

std::vector<double> CoupVariantBState::get_utilities() const {
  std::vector<double> utils(2, 0.0);
  if (!is_terminal()) {
    std::cerr << "[ERROR] get_utilities() called on a non-terminal node"
              << std::endl;
    std::exit(1);
  }
  if (current_player == 0) return {1.0, -1.0};
  if (current_player == 1) return {-1.0, 1.0};

  std::cerr << "[ERROR] get_utilities() does not return" << std::endl;
  std::exit(1);
}