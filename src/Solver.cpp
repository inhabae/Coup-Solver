#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CoupVariantAState.hpp"

class InformationSet {
 public:
  std::string key;
  std::vector<Action> legal_actions;
  std::unordered_map<Action, double> regret_sum;
  std::unordered_map<Action, double> strategy_sum;

  InformationSet(const std::string &infoset_id,
                 const std::vector<Action> &actions)
      : key(infoset_id), legal_actions(actions) {
    for (Action a : actions) {
      regret_sum[a] = 0.0;
      strategy_sum[a] = 0.0;
    }
  }

  std::unordered_map<Action, double> get_strategy() {
    std::unordered_map<Action, double> strategy;
    double normalizing_sum = 0.0;
    for (Action a : legal_actions) {
      double positive_regret = std::max(regret_sum[a], 0.0);
      normalizing_sum += positive_regret;
    }
    if (normalizing_sum > 0) {
      for (Action a : legal_actions) {
        strategy[a] = std::max(regret_sum[a], 0.0) / normalizing_sum;
      }
    } else {
      double uniform_prob = 1.0 / legal_actions.size();
      for (Action a : legal_actions) {
        strategy[a] = uniform_prob;
      }
    }
    return strategy;
  }

  std::unordered_map<Action, double> get_average_strategy() {
    std::unordered_map<Action, double> avg_strategy;
    double normalizing_sum = 0.0;
    for (Action a : legal_actions) {
      normalizing_sum += strategy_sum[a];
    }
    if (normalizing_sum > 0) {
      for (Action a : legal_actions) {
        avg_strategy[a] = strategy_sum[a] / normalizing_sum;
      }
    } else {
      double uniform_prob = 1.0 / legal_actions.size();
      for (Action a : legal_actions) {
        avg_strategy[a] = uniform_prob;
      }
    }
    return avg_strategy;
  }
};

class CFRPlusSolver {
 private:
  std::unordered_map<std::string, std::unique_ptr<InformationSet>>
      information_sets;
  int num_players;
  int delay;

  std::unordered_map<std::string, Action> br_strategy;
  std::unordered_set<std::string> visited;

  std::unordered_map<std::string, std::vector<CoupVariantAState>>
      infoset_states_;
  std::unordered_map<std::string, double> opp_reach_at_;

  InformationSet *get_information_set(const std::string &key,
                                      const std::vector<Action> &actions) {
    if (information_sets.find(key) == information_sets.end()) {
      information_sets[key] = std::make_unique<InformationSet>(key, actions);
    }
    return information_sets[key].get();
  }

 public:
  CFRPlusSolver(int delay) : delay(delay) { num_players = 2; }

  // Main CFR+ recursive function
  std::vector<double> cfr_plus_recursive(
      const CoupVariantAState &state, Player traversing_player, double weight,
      const std::vector<double> &reach_probabilities) {
    if (state.is_terminal()) {
      std::vector<double> utilities = state.get_utilities();
      std::vector<double> result(utilities.size());
      for (size_t i = 0; i < utilities.size(); ++i) {
        result[i] = utilities[i] * reach_probabilities[1 - traversing_player];
      }
      return result;
    }

    if (state.is_chance_node()) {
      std::vector<double> expected_utility(num_players, 0.0);
      std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};

      for (Card p1_card : cards) {
        for (Card p2_card : cards) {
          CoupVariantAState child_state;
          child_state.player1_card = p1_card;
          child_state.player2_card = p2_card;
          child_state.action_history.push_back(GAME_START);

          double prob = (p1_card == p2_card) ? 2.0 / 30.0 : 4.0 / 30.0;

          auto child_utilities = cfr_plus_recursive(
              child_state, traversing_player, weight, reach_probabilities);

          for (int p = 0; p < num_players; ++p) {
            expected_utility[p] += prob * child_utilities[p];
          }
        }
      }
      return expected_utility;
    }

    // Decision node
    Player current_player = state.get_current_player();
    std::vector<Action> actions = state.get_legal_actions();
    std::string infoset_key = state.get_information_set_key();

    InformationSet *infoset = get_information_set(infoset_key, actions);
    auto strategy = infoset->get_strategy();

    std::vector<double> node_utility(num_players, 0.0);
    std::unordered_map<Action, std::vector<double>> action_utilities;

    for (Action action : actions) {
      auto child_state_ptr = state.get_child(action);
      const CoupVariantAState *child_state =
          dynamic_cast<const CoupVariantAState *>(child_state_ptr.get());

      std::vector<double> new_reach_probs = reach_probabilities;
      if (current_player != traversing_player) {
        new_reach_probs[current_player] *= strategy[action];
      }

      auto child_utilities = cfr_plus_recursive(*child_state, traversing_player,
                                                weight, new_reach_probs);
      action_utilities[action] = child_utilities;

      for (int i = 0; i < num_players; ++i) {
        node_utility[i] += strategy[action] * child_utilities[i];
      }
    }

    if (current_player == traversing_player) {
      for (Action action : actions) {
        double regret = action_utilities[action][traversing_player] -
                        node_utility[traversing_player];
        infoset->regret_sum[action] =
            std::max(infoset->regret_sum[action] + regret, 0.0);
      }
    } else {
      double opponent_reach = reach_probabilities[current_player];
      for (Action action : actions) {
        infoset->strategy_sum[action] +=
            opponent_reach * strategy[action] * (weight + 1);
      }
    }

    return node_utility;
  }

  // Build infosets and opponent+chance reach in one pass over the tree
  void build_infosets_and_reach(
      const CoupVariantAState &state, Player best_response_player,
      const std::unordered_map<std::string, std::unordered_map<Action, double>>
          &opponent_strategies,
      double opp_reach) {
    // Record this state in its infoset
    std::string I = state.get_information_set_key();
    infoset_states_[I].push_back(state);
    // Record reach probability by full history key
    opp_reach_at_[state.get_history_key()] = opp_reach;

    // Terminal? stop
    if (state.is_terminal()) return;

    // Chance node: branch on card deals
    if (state.is_chance_node()) {
      std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};
      for (auto c1 : cards)
        for (auto c2 : cards) {
          double prob = (c1 == c2) ? 2.0 / 30.0 : 4.0 / 30.0;
          CoupVariantAState child = state;
          child.player1_card = c1;
          child.player2_card = c2;
          child.action_history.push_back(GAME_START);
          build_infosets_and_reach(child, best_response_player,
                                   opponent_strategies, opp_reach * prob);
        }
      return;
    }

    // Decision node
    Player to_move = state.get_current_player();
    auto actions = state.get_legal_actions();

    // Opponent node: follow their strategy
    if (to_move != best_response_player) {
      // fetch or uniform
      auto it = opponent_strategies.find(I);
      std::unordered_map<Action, double> strat;
      if (it != opponent_strategies.end())
        strat = it->second;
      else {
        double u = 1.0 / actions.size();
        for (auto a : actions) strat[a] = u;
      }
      for (auto [a, prob] : strat) {
        auto child_ptr = state.get_child(a);
        build_infosets_and_reach(*child_ptr, best_response_player,
                                 opponent_strategies, opp_reach * prob);
      }
      return;
    }

    // Best-response node: branch on all actions (opp_reach unchanged)
    for (auto a : actions) {
      auto child_ptr = state.get_child(a);
      build_infosets_and_reach(*child_ptr, best_response_player,
                               opponent_strategies, opp_reach);
    }
  }

  double calculate_best_response(
      const CoupVariantAState &state, Player best_response_player,
      const std::unordered_map<std::string, std::unordered_map<Action, double>>
          &opponent_strategies,
      double opp_reach) {
    // 1) Terminal
    if (state.is_terminal()) {
      auto util = state.get_utilities();
      return util[best_response_player];
    }

    // 2) Chance node
    if (state.is_chance_node()) {
      double exp_u = 0.0;
      std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};
      for (auto c1 : cards)
        for (auto c2 : cards) {
          CoupVariantAState child = state;
          child.player1_card = c1;
          child.player2_card = c2;
          child.action_history.push_back(GAME_START);
          double prob = (c1 == c2) ? 2.0 / 30.0 : 4.0 / 30.0;
          exp_u += prob * calculate_best_response(child, best_response_player,
                                               opponent_strategies, opp_reach);
        }
      return exp_u;
    }

    // 3) Decision node
    Player to_move = state.get_current_player();
    auto actions = state.get_legal_actions();
    auto I = state.get_information_set_key();

    // 3a) Our node: pick one action per infoset
    if (to_move == best_response_player) {
      // *first* time we see this infoset: evaluate all actions over all
      // histories in I
      if (!visited.count(I)) {
        std::unordered_map<Action, double> EV;
        for (auto a : actions) EV[a] = 0.0;

        // aggregate over every history in the infoset
        for (const auto &hist_state : infoset_states_[I]) {
          double reach = opp_reach_at_[hist_state.get_history_key()];
          for (auto a : actions) {
            auto child_ptr = hist_state.get_child(a);
            auto &child =
                *dynamic_cast<const CoupVariantAState *>(child_ptr.get());
            EV[a] +=
                reach * calculate_best_response(child, best_response_player,
                                                opponent_strategies, reach);
          }
        }

        // choose the action with max EV
        auto best = std::max_element(
            EV.begin(), EV.end(),
            [](auto &x, auto &y) { return x.second < y.second; });
        br_strategy[I] = best->first;
        visited.insert(I);
      }

      // now always play the chosen action
      Action a = br_strategy[I];
      auto child_ptr = state.get_child(a);
      auto &child = *dynamic_cast<const CoupVariantAState *>(child_ptr.get());
      return calculate_best_response(child, best_response_player,
                                     opponent_strategies, opp_reach);
    }

    // 3b) Opponent node: fold in their σ
    {
      double exp_u = 0.0;
      std::unordered_map<Action, double> σ;
      auto it = opponent_strategies.find(I);
      if (it != opponent_strategies.end())
        σ = it->second;
      else {
        double uniform = 1.0 / actions.size();
        for (auto a : actions) σ[a] = uniform;
      }

      for (auto a : actions) {
        double p = σ[a];
        auto child_ptr = state.get_child(a);
        auto &child = *dynamic_cast<const CoupVariantAState *>(child_ptr.get());
        exp_u +=
            p * calculate_best_response(child, best_response_player,
                                        opponent_strategies, opp_reach * p);
      }
      return exp_u;
    }
  }

  // Calculate expected value when both players follow their strategies
  double calculate_expected_value(
      const CoupVariantAState &state, Player target_player,
      const std::unordered_map<std::string, std::unordered_map<Action, double>>
          &strategies,
      const double reach_prob) {
    // Terminal node
    if (state.is_terminal()) {
      std::vector<double> utilities = state.get_utilities();
      return utilities[target_player];
    }

    // Chance node
    if (state.is_chance_node()) {
      double expected_utility = 0.0;

      // Iterate through all possible card assignments
      std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};
      for (Card p1_card : cards) {
        for (Card p2_card : cards) {
          CoupVariantAState child_state;
          child_state.player1_card = p1_card;
          child_state.player2_card = p2_card;
          child_state.action_history.push_back(GAME_START);

          double prob = (p1_card == p2_card) ? 2.0 / 30.0 : 4.0 / 30.0;

          double child_value = calculate_expected_value(
              child_state, target_player, strategies, reach_prob);

          expected_utility += prob * child_value;
        }
      }
      return expected_utility;
    }

    // Decision node
    std::vector<Action> actions = state.get_legal_actions();
    std::string infoset_key = state.get_information_set_key();

    double expected_value = 0.0;

    // Get strategy for current player
    auto strategy_it = strategies.find(infoset_key);
    std::unordered_map<Action, double> strategy;

    if (strategy_it != strategies.end()) {
      strategy = strategy_it->second;
    } else {
      // If no strategy found, use uniform
      std::cerr << "[INFO] No strategy found while finding expected strategy; "
                   "using uniform strategy"
                << std::endl;
      double uniform_prob = 1.0 / actions.size();
      for (Action action : actions) {
        strategy[action] = uniform_prob;
      }
    }

    for (Action action : actions) {
      double new_reach_probs = reach_prob * strategy[action];

      // std::cerr << "[DEBUG] strategy[action] = " << strategy[action] <<
      // std::endl;

      auto child_state_ptr = state.get_child(action);
      const CoupVariantAState *child_state =
          dynamic_cast<const CoupVariantAState *>(child_state_ptr.get());

      double action_prob = strategy.count(action) ? strategy[action] : 0.0;
      double action_value = calculate_expected_value(
          *child_state, target_player, strategies, new_reach_probs);

      // std::cerr << "[DEBUG] action_value: " << action_value << " with reach
      // prob: " << new_reach_probs << std::endl;
      //   if (new_reach_probs == 0) {
      //     std::cerr << "[INFO] reach_prob == 0 while finding expected
      //     strategy"
      //               << reach_prob << new_reach_probs << std::endl;
      //     std::cerr << "\taction history: [";
      //     for (size_t i = 0; i < state.action_history.size(); ++i) {
      //       std::cerr << state.action_history[i];
      //       if (i + 1 < state.action_history.size())
      //         std::cerr << ", ";
      //     }
      //     std::cerr << "] and next action " << action << std::endl;
      //   }

      expected_value += action_value * action_prob;
    }

    return expected_value;
  }

  // Get the computed strategy for an information set
  std::unordered_map<Action, double> get_strategy(
      const std::string &infoset_key) {
    if (information_sets.find(infoset_key) != information_sets.end()) {
      return information_sets[infoset_key]->get_average_strategy();
    }
    return {};
  }

  // Debug version of calculate_exploitability to trace the issue
  double calculate_exploitability(const CoupVariantAState &root_state) {
    auto strategies = get_all_strategies();
    double initial_reach_prob = 1.0;
    double init_reach_probs = 1.0;
    double total_exploitability = 0.0;

    std::cout << "\nDEBUG: Calculating exploitability..." << std::endl;
    std::cout << "Total information sets found: " << strategies.size()
              << std::endl;

    for (Player player = 0; player < num_players; ++player) {
      std::cout << "\n--- Analyzing Player " << player << " ---" << std::endl;

      br_strategy.clear();
      visited.clear();
      infoset_states_.clear();
      opp_reach_at_.clear();
      build_infosets_and_reach(root_state, player, strategies,
                               /*opp_reach=*/1.0);

      // for (const auto& pair : opp_reach_at_) {
      // std::cout << "At key : " << pair.first << ", reach is " << pair.second
      // << std::endl;
      // }

      // Calculate values with debug output
      double best_response_value = calculate_best_response(
          root_state, player, strategies, initial_reach_prob);

      double expected_value = calculate_expected_value(
          root_state, player, strategies, init_reach_probs);

      std::cout << "Player " << player
                << " best response: " << best_response_value << std::endl;
      std::cout << "Player " << player << " expected value: " << expected_value
                << std::endl;

      double player_exploitability = best_response_value - expected_value;

      if (best_response_value < expected_value) {
        std::cerr
            << "[ERROR] Best response EV is smaller than current strategy EV"
            << std::endl;
        std::exit(1);
      }
      total_exploitability += player_exploitability;

      std::cout << "Player " << player
                << " exploitability: " << player_exploitability << std::endl;
    }

    return total_exploitability;
  }

  // Print strategy in a readable format
  void print_strategy(
      const std::unordered_map<std::string, std::unordered_map<Action, double>>
          &strategy,
      const std::string &strategy_name) {
    std::cout << "\n=== " << strategy_name << " ===" << std::endl;

    for (const auto &infoset_pair : strategy) {
      std::cout << "Information Set: " << infoset_pair.first << std::endl;

      for (const auto &action_pair : infoset_pair.second) {
        std::cout << "  Action " << action_pair.first << ": " << std::fixed
                  << std::setprecision(4) << action_pair.second << std::endl;
      }
      std::cout << std::endl;
    }
  }

  // Enhanced training method with exploitability tracking and early stopping
  void train_with_exploitability_tracking(const CoupVariantAState &root_state,
                                          int max_iterations,
                                          int check_interval,
                                          double target_exploitability) {
    std::cout << "Training with exploitability tracking..." << std::endl;
    std::cout << "Max iterations: " << max_iterations << std::endl;
    std::cout << "Target exploitability: " << target_exploitability
              << std::endl;
    std::cout << "Check interval: " << check_interval << std::endl;

    for (int t = 1; t <= max_iterations; ++t) {
      double weight = std::max(t - delay, 0);

      for (Player player = 0; player < num_players; ++player) {
        std::vector<double> reach_probs(num_players, 1.0);
        cfr_plus_recursive(root_state, player, weight, reach_probs);
      }

      if (t % check_interval == 0) {
        std::cout << "\n=== Iteration " << t << " ===" << std::endl;
        double exploitability = calculate_exploitability(root_state);
        std::cout << "Exploitability at iteration " << t << ": "
                  << exploitability << std::endl;

        // Check if we've reached the target exploitability
        if (exploitability <= target_exploitability) {
          std::cout << "*** TARGET EXPLOITABILITY REACHED ***" << std::endl;
          std::cout << "Stopping early at iteration " << t << std::endl;
          std::cout << "Final exploitability: " << exploitability << std::endl;
          break;
        }
      }
    }
  }

  // Get all computed strategies
  std::unordered_map<std::string, std::unordered_map<Action, double>>
  get_all_strategies() {
    std::unordered_map<std::string, std::unordered_map<Action, double>>
        strategies;
    for (const auto &pair : information_sets) {
      strategies[pair.first] = pair.second->get_average_strategy();
    }
    return strategies;
  }

  // BINARY STORAGE METHODS
  void save_strategies_binary(const std::string &filename) {
    auto strategies = get_all_strategies();

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    // Write number of information sets
    size_t num_infosets = strategies.size();
    file.write(reinterpret_cast<const char *>(&num_infosets),
               sizeof(num_infosets));

    for (const auto &infoset_pair : strategies) {
      // Write information set key length and key
      size_t key_length = infoset_pair.first.length();
      file.write(reinterpret_cast<const char *>(&key_length),
                 sizeof(key_length));
      file.write(infoset_pair.first.c_str(), key_length);

      // Write number of actions
      size_t num_actions = infoset_pair.second.size();
      file.write(reinterpret_cast<const char *>(&num_actions),
                 sizeof(num_actions));

      // Write action-probability pairs
      for (const auto &action_pair : infoset_pair.second) {
        file.write(reinterpret_cast<const char *>(&action_pair.first),
                   sizeof(action_pair.first));
        file.write(reinterpret_cast<const char *>(&action_pair.second),
                   sizeof(action_pair.second));
      }
    }

    file.close();
    std::cout << "Strategies saved to binary file: " << filename << std::endl;
  }

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
    std::cout << "Strategies loaded from binary file: " << filename
              << std::endl;
    return strategies;
  }
};

// Main function demonstrating usage
// Updated main function to demonstrate exploitability-based early stopping
int main() {
  // Create game instance
  CoupVariantAState root_state;

  // Create CFR+ solver
  CFRPlusSolver solver(100);

  // Example 1: Train with simple early stopping
  std::cout << "\n=== Training with Early Stopping ===" << std::endl;
  solver.train_with_exploitability_tracking(
      root_state, 10000000, 1000, 0.01);  // Stop if exploitability <= 0.01

  // Example 2: Alternative - train until converged with patience
  // std::cout << "\n=== Training Until Converged ===" << std::endl;
  // solver.train_until_converged(root_state, 50000, 0.001, 1000, 3); // Stop if
  // exploitability <= 0.001 for 3 consecutive checks

  // Calculate final exploitability
  std::cout << "\n=== Final Results ===" << std::endl;
  double final_exploitability = solver.calculate_exploitability(root_state);

  // Save strategies
  solver.save_strategies_binary("strategies.bin");

  std::cout << "\nFinal exploitability: " << std::fixed << std::setprecision(6)
            << final_exploitability << std::endl;
  std::cout
      << "Lower exploitability indicates better Nash equilibrium approximation."
      << std::endl;

  // solver.print_final_strategy_comparison(root_state);

//   auto current_strategies = solver.get_all_strategies();
//   solver.print_strategy(current_strategies, "Final Average Strategies");

  return 0;
}