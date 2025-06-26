#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Action = int;

// Action name mappings based on CoupVariantAState.hpp
const std::unordered_map<int, std::string> ACTION_NAMES = {
    {0, "Game start"},  {1, "Income"}, {2, "Tax"},
    {3, "Assassinate"}, {4, "Coup"},   {5, "Block Assassinate"},
    {6, "Challenge"},   {7, "Pass"},   {8, "Lose card"},
    {9, "Show card"}};

// Card name mappings based on CoupVariantAState.hpp
const std::vector<std::string> CARD_NAMES = {"ASSASSIN", "CONTESSA", "DUKE"};
const std::vector<std::string> PLAYER_PREFIXES = {"P1", "P2"};

// Function to get action name
std::string get_action_name(int action) {
  auto it = ACTION_NAMES.find(action);
  return (it != ACTION_NAMES.end())
             ? it->second
             : "Unknown action " + std::to_string(action);
}

// Function to load strategies from binary file (matching Solver.cpp format)
std::unordered_map<std::string, std::unordered_map<Action, double>>
load_strategies_binary(const std::string &filename) {

  std::unordered_map<std::string, std::unordered_map<Action, double>>
      strategies;
  std::ifstream file(filename, std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file for reading: " + filename);
  }

  // Read number of information sets
  size_t num_infosets;
  file.read(reinterpret_cast<char *>(&num_infosets), sizeof(num_infosets));

  for (size_t i = 0; i < num_infosets; ++i) {
    // Read information set key
    size_t key_length;
    file.read(reinterpret_cast<char *>(&key_length), sizeof(key_length));

    std::string key(key_length, '\0');
    file.read(&key[0], key_length);

    // Read number of actions
    size_t num_actions;
    file.read(reinterpret_cast<char *>(&num_actions), sizeof(num_actions));

    // Read action-probability pairs
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
  std::cout << "Strategies loaded from binary file: " << filename << std::endl;
  std::cout << "Total information sets loaded: " << strategies.size()
            << std::endl;
  return strategies;
}

// Function to revert last action from infoset key
std::string revert_action(const std::string &infoset_key) {
  // Expected format: P1:ASSASSIN: INCOME TAX ASSASSINATE
  size_t first_colon = infoset_key.find(':');
  size_t second_colon = infoset_key.find(':', first_colon + 1);

  if (first_colon == std::string::npos || second_colon == std::string::npos) {
    // Invalid key or no actions
    return infoset_key;
  }

  // Extract player and card
  std::string player_card =
      infoset_key.substr(0, second_colon + 1); // e.g., "P1:ASSASSIN:"

  // Get action sequence
  std::string action_part = infoset_key.substr(second_colon + 1);
  if (action_part.empty() || action_part == " ") {
    // No actions, return player and card with initial action
    return player_card + " 0";
  }

  // Find the last space
  size_t last_space = action_part.find_last_of(' ');
  if (last_space == std::string::npos || last_space == 0) {
    // Only one action or no spaces, revert to initial action
    return player_card + " 0";
  }

  // Remove the last action
  return player_card + " " + action_part.substr(0, last_space);
}

// Function to add action to infoset key
std::string do_action(const std::string &infoset_key, int action) {
  // Expected format: P1:ASSASSIN: [ACTIONS]
  size_t second_colon = infoset_key.find(':', infoset_key.find(':') + 1);
  if (second_colon == std::string::npos) {
    return infoset_key; // Invalid key, return unchanged
  }

  std::string action_part = infoset_key.substr(second_colon + 1);
  if (action_part.empty() || action_part == " ") {
    // No actions yet, add the new one
    return infoset_key + std::to_string(action);
  }

  // Append action with a space
  return infoset_key + " " + std::to_string(action);
}

// Function to extract action sequence from infoset key
std::string get_action_sequence(const std::string &infoset_key) {
  // Expected format: P1:ASSASSIN: [ACTIONS]
  size_t second_colon = infoset_key.find(':', infoset_key.find(':') + 1);
  if (second_colon == std::string::npos) {
    return "";
  }

  std::string action_part = infoset_key.substr(second_colon + 1);
  if (action_part.empty() || action_part == " ") {
    return "";
  }

  // Remove leading space if present
  if (action_part[0] == ' ') {
    return action_part.substr(1);
  }
  return action_part;
}

// Function to display strategy for all card holdings at a given action sequence
void display_strategy_all_cards(
    const std::unordered_map<std::string, std::unordered_map<int, double>>
        &strategies,
    const std::string &action_sequence) {

  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Strategy for action sequence: " << action_sequence << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Check strategies for each player and card
  for (const auto &player : PLAYER_PREFIXES) {
    for (const auto &card : CARD_NAMES) {
      std::string infoset_key = player + ":" + card + ": " + action_sequence;

      std::cout << "\n"
                << player << " with " << card << " (" << infoset_key
                << "):" << std::endl;
      std::cout << std::string(40, '-') << std::endl;

      auto it = strategies.find(infoset_key);
      if (it != strategies.end()) {
        const auto &strategy = it->second;

        // Display each action probability with names
        for (const auto &action_prob : strategy) {
          std::cout << "  " << get_action_name(action_prob.first) << ": "
                    << std::fixed << std::setprecision(4) << action_prob.second
                    << std::endl;
        }
      } else {
        std::cout << "  No strategy found for this infoset" << std::endl;
      }
    }
  }
  std::cout << std::endl;
}

// Function to display available actions at current state
void display_available_actions(
    const std::unordered_map<std::string, std::unordered_map<int, double>>
        &strategies,
    const std::string &action_sequence) {

  std::cout << "\nAvailable actions from current state:" << std::endl;
  std::cout << std::string(40, '-') << std::endl;

  // Get available actions by checking what actions exist for any card at this
  // sequence
  std::set<int> available_actions;

  for (const auto &player : PLAYER_PREFIXES) {
    for (const auto &card : CARD_NAMES) {
      std::string infoset_key = player + ":" + card + ": " + action_sequence;
      auto it = strategies.find(infoset_key);
      if (it != strategies.end()) {
        for (const auto &action_prob : it->second) {
          available_actions.insert(action_prob.first);
        }
      }
    }
  }

  if (available_actions.empty()) {
    std::cout << "  No actions available (terminal state or no data)"
              << std::endl;
  } else {
    for (int action : available_actions) {
      std::cout << "  " << action << ": " << get_action_name(action)
                << std::endl;
    }
  }
  std::cout << std::endl;
}

// Function to display action history
void display_action_history(const std::string &action_sequence) {
  std::cout << "\nAction History:" << std::endl;
  std::cout << std::string(40, '-') << std::endl;

  if (action_sequence.empty()) {
    std::cout << "  (Game start - no actions yet)" << std::endl;
    return;
  }

  // Parse action sequence
  std::stringstream ss(action_sequence);
  std::string action_str;
  int step = 0;

  while (std::getline(ss, action_str, ' ')) {
    if (!action_str.empty()) {
      int action = std::stoi(action_str);
      std::cout << "  " << step << ": " << get_action_name(action)
                << " (Action " << action << ")" << std::endl;
      step++;
    }
  }
  std::cout << std::endl;
}

// Interactive function to explore strategies continuously
void interactive_strategy_explorer(
    const std::unordered_map<std::string, std::unordered_map<int, double>>
        &strategies) {

  std::string current_sequence = "0"; // Start with just the initial action
  std::string input;

  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "INTERACTIVE STRATEGY EXPLORER" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  <number>  : Add action to sequence" << std::endl;
  std::cout << "  r         : Revert last action" << std::endl;
  std::cout << "  s         : Show current strategy" << std::endl;
  std::cout << "  h         : Show action history" << std::endl;
  std::cout << "  a         : Show available actions" << std::endl;
  std::cout << "  reset     : Reset to game start" << std::endl;
  std::cout << "  help      : Show this help" << std::endl;
  std::cout << "  q         : Quit" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Show initial state
  display_action_history(current_sequence);
  display_strategy_all_cards(strategies, current_sequence);
  display_available_actions(strategies, current_sequence);

  while (true) {
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "Current sequence: " << current_sequence << std::endl;
    std::cout << "Enter command: ";
    std::cin >> input;

    if (input == "q") {
      std::cout << "Goodbye!" << std::endl;
      break;
    } else if (input == "r") {
      // Create a sample infoset key to test revert
      std::string sample_key = "P1:ASSASSIN: " + current_sequence;
      std::string reverted_key = revert_action(sample_key);
      std::string new_sequence = get_action_sequence(reverted_key);

      if (new_sequence != current_sequence) {
        current_sequence = new_sequence;
        std::cout << "Reverted to: " << current_sequence << std::endl;
        display_action_history(current_sequence);
        display_strategy_all_cards(strategies, current_sequence);
        display_available_actions(strategies, current_sequence);
      } else {
        std::cout << "Cannot revert further - already at minimum state"
                  << std::endl;
      }
    } else if (input == "s") {
      display_strategy_all_cards(strategies, current_sequence);
    } else if (input == "h") {
      display_action_history(current_sequence);
    } else if (input == "a") {
      display_available_actions(strategies, current_sequence);
    } else if (input == "reset") {
      current_sequence = "0";
      std::cout << "Reset to game start" << std::endl;
      display_action_history(current_sequence);
      display_strategy_all_cards(strategies, current_sequence);
      display_available_actions(strategies, current_sequence);
    } else if (input == "help") {
      std::cout << "\nCommands:" << std::endl;
      std::cout << "  <number>  : Add action to sequence" << std::endl;
      std::cout << "  r         : Revert last action" << std::endl;
      std::cout << "  s         : Show current strategy" << std::endl;
      std::cout << "  h         : Show action history" << std::endl;
      std::cout << "  a         : Show available actions" << std::endl;
      std::cout << "  reset     : Reset to game start" << std::endl;
      std::cout << "  help      : Show this help" << std::endl;
      std::cout << "  q         : Quit" << std::endl;
    } else {
      // Try to parse as action number
      try {
        int action = std::stoi(input);
        std::string sample_key = "P1:ASSASSIN: " + current_sequence;
        std::string new_key = do_action(sample_key, action);
        current_sequence = get_action_sequence(new_key);

        std::cout << "Added " << get_action_name(action) << " (Action "
                  << action << ")" << std::endl;
        std::cout << "New sequence: " << current_sequence << std::endl;

        display_action_history(current_sequence);
        display_strategy_all_cards(strategies, current_sequence);
        display_available_actions(strategies, current_sequence);

      } catch (const std::exception &e) {
        std::cout << "Invalid command. Type 'help' for available commands"
                  << std::endl;
      }
    }
  }
}

int main() {
  std::cout << "Coup Variant A Strategy Viewer" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "Action Mappings:" << std::endl;
  for (const auto &pair : ACTION_NAMES) {
    std::cout << "  " << pair.first << ": " << pair.second << std::endl;
  }
  std::cout << std::string(60, '=') << std::endl;

  std::string filename = "strategies.bin";

  try {
    // Load strategies from binary file
    auto strategies = load_strategies_binary(filename);

    if (strategies.empty()) {
      std::cout << "No strategies found in file. Exiting." << std::endl;
      return 1;
    }

    // Run interactive explorer continuously
    interactive_strategy_explorer(strategies);

  } catch (const std::exception &e) {
    std::cerr << "Error loading strategies: " << e.what() << std::endl;
    std::cerr
        << "Make sure the binary file exists and was created by your Solver.cpp"
        << std::endl;
    return 1;
  }

  return 0;
}