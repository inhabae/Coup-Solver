#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include "RPSState.hpp"

// Function to get action name
std::string get_action_name(Action action) {
  static const std::unordered_map<Action, std::string> ACTION_NAMES = {
      {GAME_START, "GAME_START"},
      {ROCK, "ROCK"},
      {PAPER, "PAPER"},
      {SCISSORS, "SCISSORS"}};
  auto it = ACTION_NAMES.find(action);
  return it != ACTION_NAMES.end()
             ? it->second
             : "Unknown action " + std::to_string(static_cast<int>(action));
}

// Function to load strategies from binary file
std::unordered_map<std::string, std::unordered_map<Action, double>>
load_strategies_binary(const std::string &filename) {
  std::unordered_map<std::string, std::unordered_map<Action, double>>
      strategies;
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  size_t num_infosets;
  file.read(reinterpret_cast<char *>(&num_infosets), sizeof(num_infosets));

  for (size_t i = 0; i < num_infosets; ++i) {
    size_t key_length;
    file.read(reinterpret_cast<char *>(&key_length), sizeof(key_length));
    std::string key(key_length, '\0');
    file.read(&key[0], key_length);
    size_t num_actions;
    file.read(reinterpret_cast<char *>(&num_actions), sizeof(num_actions));
    std::unordered_map<Action, double> action_probs;
    for (size_t j = 0; j < num_actions; ++j) {
      Action action;
      double prob;
      file.read(reinterpret_cast<char *>(&action), sizeof(action));
      file.read(reinterpret_cast<char *>(&prob), sizeof(prob));
      action_probs[action] = prob;
    }
    strategies[key] = action_probs;
  }
  file.close();
  return strategies;
}

int main() {
  std::cout << "Loading strategies from rps_strategies.bin\n"
            << std::string(50, '-') << "\n";
  try {
    auto strategies = load_strategies_binary("rps_strategies.bin");
    std::cout << "Dictionary Contents (" << strategies.size()
              << " information sets):\n"
              << std::string(50, '-') << "\n";
    for (const auto &kv : strategies) {
      std::cout << "Key: " << kv.first << "\n";
      std::cout << "  Actions:\n";
      for (const auto &action_prob : kv.second) {
        std::cout << "    " << get_action_name(action_prob.first) << ": "
                  << std::fixed << std::setprecision(3) << action_prob.second
                  << "\n";
      }
      std::cout << "\n";
    }
    std::cout << std::string(50, '-') << "\n";
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what()
              << "\nEnsure 'rps_strategies.bin' exists and matches the "
                 "expected format.\n";
    return 1;
  }
  return 0;
}