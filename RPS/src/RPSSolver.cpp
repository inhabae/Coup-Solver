#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "RPSState.hpp"

double init_prob = 0.5;

template <typename T>
inline void hash_combine(std::size_t &seed, const T &val) {
  seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline size_t get_information_set_hash(const RPSState &state) {
  size_t h = 0;
  hash_combine(h, state.get_current_player());
  hash_combine(h, state.get_current_player() == 0 ? state.player1_card
                                                  : state.player2_card);
  for (size_t s = 0; s < state.action_history.size(); ++s)
    hash_combine(h, static_cast<int>(1)); // In order to hide action
  return h;
}

inline size_t get_history_hash(const RPSState &state) {
  size_t h = 0;
  hash_combine(h, state.player1_card);
  hash_combine(h, state.player2_card);
  for (auto a : state.action_history)
    hash_combine(h, static_cast<int>(a));
  return h;
}

// Fixed-size InformationSet for CFR+
class InformationSet {
public:
  static constexpr int MAX_ACTIONS = 16;
  size_t key;
  Action legal[MAX_ACTIONS];
  int num_actions;
  double regret_sum[MAX_ACTIONS];
  double strat_sum[MAX_ACTIONS];
  double strategy[MAX_ACTIONS];
  double avg_strategy[MAX_ACTIONS];
  bool strat_dirty;
  bool avg_dirty;

  InformationSet(size_t id, const std::vector<Action> &actions)
      : key(id), num_actions(actions.size()), strat_dirty(true),
        avg_dirty(true) {
    for (int i = 0; i < num_actions; ++i) {
      legal[i] = actions[i];
      regret_sum[i] = strat_sum[i] = strategy[i] = avg_strategy[i] = 0.0;
    }
  }

  // Regret-matching strategy
  const double *get_strategy() {
    if (strat_dirty) {
      double sum_pos_regret = 0;
      for (int i = 0; i < num_actions; ++i)
        sum_pos_regret += std::max(regret_sum[i], 0.0);
      if (sum_pos_regret > 0) {
        for (int i = 0; i < num_actions; ++i)
          strategy[i] = std::max(regret_sum[i], 0.0) / sum_pos_regret;
      } else {
        double u = 1.0 / num_actions;
        for (int i = 0; i < num_actions; ++i)
          strategy[i] = u;
      }
      strat_dirty = false;
    }
    return strategy;
  }

  // Average strategy
  const double *get_average_strategy() {
    if (avg_dirty) {
      double sum = std::accumulate(strat_sum, strat_sum + num_actions, 0.0);
      if (sum > 0) {
        for (int i = 0; i < num_actions; ++i)
          avg_strategy[i] = strat_sum[i] / sum;
      } else {
        double u = 1.0 / num_actions;
        for (int i = 0; i < num_actions; ++i)
          avg_strategy[i] = u;
      }
      avg_dirty = false;
    }
    return avg_strategy;
  }

  void update_regret(int idx, double r) {
    regret_sum[idx] = std::max(regret_sum[idx] + r, 0.0);
    strat_dirty = true;
  }

  void update_strategy_sum(int idx, double v) {
    strat_sum[idx] += v;
    avg_dirty = true;
  }
};

class CFRPlusSolver {
private:
  std::unordered_map<size_t, std::unique_ptr<InformationSet>> infosets;
  std::unordered_map<size_t, std::string> hash2key;
  std::unordered_map<std::string, size_t> key2hash;
  std::unordered_map<size_t, std::vector<RPSState>> iset_states;
  std::unordered_map<size_t, double> opp_reach;
  std::unordered_map<size_t, Action> br_strategy;
  std::unordered_map<size_t, Action> br_strategy_1;

  std::unordered_map<std::string, Action> br_strategy_i;
  std::unordered_set<size_t> visited;
  int num_players = 2;
  int delay;

  InformationSet *get_iset(const RPSState &state) {
    size_t h = get_information_set_hash(state);
    auto it = infosets.find(h);
    if (it == infosets.end())
      throw std::runtime_error("Missing info set");
    return it->second.get();
  }

  // Precompute all information sets by walking the tree
  void precompute(const RPSState &state) {
    if (state.is_terminal())
      return;
    if (state.is_chance_node()) {
      std::vector<Card> C{CARD1, CARD2};
      for (auto c1 : C)
        for (auto c2 : C) {
          if (c1 == c2)
            continue;
          RPSState child = state;
          child.player1_card = c1;
          child.player2_card = c2;
          child.action_history.push_back(GAME_START);
          precompute(child);
        }
      return;
    }
    size_t h = get_information_set_hash(state);
    if (!infosets.count(h)) {
      auto acts = state.get_legal_actions();
      infosets[h] = std::make_unique<InformationSet>(h, acts);
      hash2key[h] = state.get_information_set_key();
      key2hash[state.get_information_set_key()] = h;
    }
    for (auto a : state.get_legal_actions()) {
      auto child_ptr = state.get_child(a);
      auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
      precompute(child);
    }
  }

public:
  CFRPlusSolver(int d) : delay(d) { infosets.reserve(4096); }

  void initialize(const RPSState &root) {
    std::cout << "Precomputing info sets...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    precompute(root);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "Done. " << infosets.size() << " sets in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                     .count()
              << "ms\n";
  }

  // Core CFR+ recursion
  std::vector<double> cfr_plus_recursive(const RPSState &state, Player trav,
                                         double weight,
                                         const std::vector<double> &reach) {
    if (state.is_terminal()) {
      auto U = state.get_utilities();
      std::vector<double> ret(U.size());
      for (size_t i = 0; i < U.size(); ++i)
        ret[i] = U[i] * reach[1 - trav];
      return ret;
    }
    if (state.is_chance_node()) {
      std::vector<double> EU(num_players, 0.0);
      std::vector<Card> C{CARD1, CARD2};
      for (auto c1 : C)
        for (auto c2 : C) {
          if (c1 == c2)
            continue;
          RPSState child = state;
          child.player1_card = c1;
          child.player2_card = c2;
          child.action_history.push_back(GAME_START);
          auto cu = cfr_plus_recursive(child, trav, weight, reach);
          for (int p = 0; p < num_players; ++p)
            EU[p] += init_prob * cu[p];
        }
      return EU;
    }
    Player cur = state.get_current_player();
    auto iset = get_iset(state);
    const double *strat = iset->get_strategy();
    std::vector<double> node_util(num_players, 0.0);
    double action_util[InformationSet::MAX_ACTIONS][2];
    for (int i = 0; i < iset->num_actions; ++i) {
      Action a = iset->legal[i];
      auto child_ptr = state.get_child(a);
      auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
      auto new_reach = reach;
      if (cur != trav)
        new_reach[cur] *= strat[i];
      auto cu = cfr_plus_recursive(child, trav, weight, new_reach);
      action_util[i][0] = cu[0];
      action_util[i][1] = cu[1];
      for (int p = 0; p < num_players; ++p)
        node_util[p] += strat[i] * cu[p];
    }
    if (cur == trav) {
      for (int i = 0; i < iset->num_actions; ++i) {
        double r = action_util[i][trav] - node_util[trav];
        iset->update_regret(i, r);
      }
    } else {
      double opp = reach[cur];
      for (int i = 0; i < iset->num_actions; ++i)
        iset->update_strategy_sum(i, opp * strat[i] * (weight + 1));
    }
    return node_util;
  }

  // Calculate expected value following given strategies
  double calculate_expected_value(
      const RPSState &state, Player target,
      const std::unordered_map<std::string, std::unordered_map<Action, double>>
          &strat_map,
      double reach_prob) {
    if (state.is_terminal()) {
      auto U = state.get_utilities();
      return U[target];
    }
    if (state.is_chance_node()) {
      double util = 0;
      std::vector<Card> C{CARD1, CARD2};
      for (auto c1 : C)
        for (auto c2 : C)
          if (c1 != c2) {
            RPSState child = state;
            child.player1_card = c1;
            child.player2_card = c2;
            child.action_history.push_back(GAME_START);
            util += init_prob * calculate_expected_value(child, target,
                                                         strat_map, reach_prob);
          }
      return util;
    }
    auto actions = state.get_legal_actions();
    auto key = state.get_information_set_key();
    std::unordered_map<Action, double> σ;
    auto it = strat_map.find(key);
    if (it != strat_map.end())
      σ = it->second;
    else {
      std::cout << "[INFO] strat_map no value\n";
      double u = 1.0 / actions.size();
      for (auto a : actions)
        σ[a] = u;
    }
    double ev = 0;
    for (auto a : actions) {
      auto child_ptr = state.get_child(a);
      auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
      ev +=
          σ[a] * calculate_expected_value(child, target, strat_map, reach_prob);
    }
    return ev;
  }

  // Compute exploitability
  double calculate_exploitability(const RPSState &root) {
    auto strat_map = get_all_strategies();
    // build hashed opponent strategies
    std::unordered_map<size_t, std::unordered_map<Action, double>> opp_strat_h;
    for (auto &kv : strat_map) {
      auto it = key2hash.find(kv.first);
      if (it == key2hash.end())
        continue;
      opp_strat_h[it->second] = kv.second;
    }
    double total = 0;
    for (int player = 0; player < num_players; ++player) {
      // clear best-response data
      br_strategy.clear();
      visited.clear();
      iset_states.clear();
      opp_reach.clear();
      br_strategy_i.clear();
      br_strategy_1.clear();
      // build tree for best response
      double best_val = calculate_best_response(root, player, opp_strat_h);
      std::cout << "Player" << player + 1 << std::endl;
      std::cout << "\tBR EV: " << best_val << std::endl;
      double exp_val = calculate_expected_value(root, player, strat_map, 1.0);
      std::cout << "\tCurrent EV: " << exp_val << std::endl;

      if (best_val < exp_val) {
        std::cerr << "[ERROR] Best response strategy EV is lower than current "
                     "strategy EV"
                  << std::endl;
        std::exit(1);
      }
      total += best_val - exp_val;
    }
    return total;
  }

  double calculate_best_response(
      const RPSState &state, Player maximizing_player,
      const std::unordered_map<size_t, std::unordered_map<Action, double>>
          &hash_strat_map,
      int depth = 0, Card c = CARD3) {

    for (int i = 0; i < depth; i++) {
      std::cout << "    ";
    }
    std::cout << "[DEBUG]" << "Player: " << maximizing_player
              << " p1 card: " << state.player1_card
              << " p2 card: " << state.player2_card
              << " Action history size: " << state.action_history.size()
              << std::endl;

    if (state.is_terminal()) {
      auto utilities = state.get_utilities();
      return utilities[maximizing_player];
    }

    if (state.is_chance_node()) {
      double expected_value = 0.0;
      std::vector<Card> cards{CARD1, CARD2};

      for (auto c1 : cards) {
        for (auto c2 : cards)
          if (c1 != c2) {
            RPSState child = state;
            child.player1_card = c1;
            child.player2_card = c2;
            child.action_history.push_back(GAME_START);

            double child_value = calculate_best_response(
                child, maximizing_player, hash_strat_map, depth + 1);

            std::cout << "[DEBUG] With p1: " << c1 << " and p2: " << c2
                      << ", EV = " << child_value << std::endl;
            expected_value += init_prob * child_value;
          }
      }
      return expected_value;
    }

    Player current_player = state.get_current_player();
    auto legal_actions = state.get_legal_actions();

    Card max_player_card =
        (maximizing_player == 0) ? state.player1_card : state.player2_card;

    if (current_player == maximizing_player) {
      // For maximizing player: find best action for this information set
      size_t iset_hash = get_information_set_hash(state);

      // Check if we've already computed best action for this information set
      if (maximizing_player == 0) {
        auto br_it = br_strategy.find(iset_hash);
        if (br_it != br_strategy.end()) {
          std::cout << "[INFO]: infoset found\n";
          Action best_action = br_it->second;
          auto child_ptr = state.get_child(best_action);
          auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
          return calculate_best_response(child, maximizing_player,
                                         hash_strat_map, depth + 1,
                                         state.player1_card);
        }
      } else {
        auto br_it = br_strategy_1.find(iset_hash);
        if (br_it != br_strategy_1.end()) {
          std::cout << "[INFO]: infoset found\n";
          Action best_action = br_it->second;
          auto child_ptr = state.get_child(best_action);
          auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
          return calculate_best_response(child, maximizing_player,
                                         hash_strat_map, depth + 1,
                                         state.player2_card);
        }
      }

      // Compute best action by evaluating each action's expected value
      // across all possible states in this information set
      double best_value = -std::numeric_limits<double>::infinity();
      Action best_action = legal_actions[0];

      for (auto action : legal_actions) {
        double action_value = 0.0;

        // We need to compute expected value of this action across all possible
        // opponent private information (since this is what determines the
        // information set)
        std::vector<Card> opp_cards{CARD1, CARD2};
        int count = 0;

        for (auto opp_card : opp_cards) {
          // Create a state with this opponent card configuration
          RPSState test_state = state;
          if (maximizing_player == 0 && opp_card != state.player1_card) {
            test_state.player2_card = opp_card;
          } else if (maximizing_player == 1 && opp_card != state.player2_card) {
            test_state.player1_card = opp_card;
          }

          // Verify this state has the same information set
          if (get_information_set_hash(test_state) == iset_hash) {
            auto child_ptr = test_state.get_child(action);
            auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
            action_value += calculate_best_response(child, maximizing_player,
                                                    hash_strat_map, depth + 1,
                                                    max_player_card);
            count++;
          }
        }

        if (count > 0) {
          action_value /= count; // Average over possible opponent cards
        } else {
          // Fallback: just use current state
          auto child_ptr = state.get_child(action);
          auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
          action_value =
              calculate_best_response(child, maximizing_player, hash_strat_map,
                                      depth + 1, max_player_card);
        }

        if (action_value > best_value) {
          best_value = action_value;
          best_action = action;
        }
      }

      // Store best action for this information set
      if (maximizing_player == 0)
        br_strategy[iset_hash] = best_action;
      if (maximizing_player == 1)
        br_strategy_1[iset_hash] = best_action;

      br_strategy_i[state.get_information_set_key()] = best_action;

      // Return value for the current state using best action
      auto child_ptr = state.get_child(best_action);
      auto &child = *dynamic_cast<RPSState *>(child_ptr.get());

      return calculate_best_response(child, maximizing_player, hash_strat_map,
                                     depth + 1, max_player_card);

    } else {
      // Opponent plays according to their strategy
      size_t iset_hash = get_information_set_hash(state);

      std::unordered_map<Action, double> strategy;
      auto strat_it = hash_strat_map.find(iset_hash);
      if (strat_it != hash_strat_map.end()) {
        strategy = strat_it->second;
        // } else {
        //     // Default to uniform strategy
        //     double uniform_prob = 1.0 / legal_actions.size();
        //     for (auto action : legal_actions) {
        //         strategy[action] = uniform_prob;
        //     }
        // }

        double expected_value = 0.0;
        for (auto action : legal_actions) {
          auto child_ptr = state.get_child(action);
          auto &child = *dynamic_cast<const RPSState *>(child_ptr.get());
          double child_value =
              calculate_best_response(child, maximizing_player, hash_strat_map,
                                      depth + 1, max_player_card);

          auto prob_it = strategy.find(action);
          double prob = (prob_it != strategy.end()) ? prob_it->second : 0.0;
          expected_value += prob * child_value;
        }

        return expected_value;
      }
    }
    std::cout << "ERROR";
    std::exit(1);
  }

  // Train with exploitability tracking
  void train_with_exploitability_tracking(const RPSState &root, int max_iter,
                                          int check_interval,
                                          double target_exploit) {
    std::cout << "Starting training...\n";
    for (int t = 1; t <= max_iter; ++t) {
      double w = std::max(t - delay, 0);
      for (int p = 0; p < num_players; ++p) {
        std::vector<double> reach(num_players, 1.0);
        cfr_plus_recursive(root, p, w, reach);
      }
      if (t % check_interval == 0) {
        double e = calculate_exploitability(root);
        std::cout << "Iteration " << t << ": exploitability = " << e << "\n";
        if (e <= target_exploit)
          break;
      }
    }
  }

  // Extract all average strategies
  std::unordered_map<std::string, std::unordered_map<Action, double>>
  get_all_strategies() {
    std::unordered_map<std::string, std::unordered_map<Action, double>> strat;
    for (auto &kv : infosets) {
      size_t h = kv.first;
      auto *iset = kv.second.get();
      const double *avg = iset->get_average_strategy();
      auto &m = strat[hash2key[h]];
      for (int i = 0; i < iset->num_actions; ++i)
        m[iset->legal[i]] = avg[i];
    }
    return strat;
  }

  // Binary save/load
  void save_strategies_binary(const std::string &filename) {
    auto strat = get_all_strategies();
    std::ofstream file(filename, std::ios::binary);
    size_t n = strat.size();
    file.write(reinterpret_cast<const char *>(&n), sizeof(n));
    for (auto &kv : strat) {
      const auto &key = kv.first;
      size_t len = key.size();
      file.write(reinterpret_cast<const char *>(&len), sizeof(len));
      file.write(key.data(), len);
      auto &m = kv.second;
      size_t na = m.size();
      file.write(reinterpret_cast<const char *>(&na), sizeof(na));
      for (auto &ap : m) {
        file.write(reinterpret_cast<const char *>(&ap.first), sizeof(ap.first));
        file.write(reinterpret_cast<const char *>(&ap.second),
                   sizeof(ap.second));
      }
    }
  }

  std::unordered_map<std::string, std::unordered_map<Action, double>>
  load_strategies_binary(const std::string &filename) {
    std::unordered_map<std::string, std::unordered_map<Action, double>> strat;
    std::ifstream file(filename, std::ios::binary);
    size_t n;
    file.read(reinterpret_cast<char *>(&n), sizeof(n));
    while (n--) {
      size_t len;
      file.read(reinterpret_cast<char *>(&len), sizeof(len));
      std::string key(len, '\0');
      file.read(&key[0], len);
      size_t na;
      file.read(reinterpret_cast<char *>(&na), sizeof(na));
      for (size_t i = 0; i < na; ++i) {
        Action a;
        double p;
        file.read(reinterpret_cast<char *>(&a), sizeof(a));
        file.read(reinterpret_cast<char *>(&p), sizeof(p));
        strat[key][a] = p;
      }
    }
    return strat;
  }
};

int main() {
  RPSState root;
  CFRPlusSolver solver(100);
  solver.initialize(root);
  solver.train_with_exploitability_tracking(root, 1000000, 10000, 0.0001);
  auto strat = solver.get_all_strategies();
  // e.g., save for later
  solver.save_strategies_binary("rps_strategies.bin");
  std::cout << "Training complete.\n";
  return 0;
}
