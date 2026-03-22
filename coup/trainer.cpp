#include "trainer.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <sstream>

Trainer::Trainer() : current_utility(0.0) {}

std::vector<double> Trainer::get_strategy(size_t infoset) {
        const std::vector<double>& regrets = regret_sum[infoset];
        std::vector<double> strategy_vec(regrets.size(), 0.0);
        double regret_total = 0.0;
        for (size_t i = 0; i < regrets.size(); i++) {
            if (regrets[i] > 0) {
                regret_total += regrets[i];
            }
        }
        if (regret_total > 0) {
            for (size_t i = 0; i < regrets.size(); i++) {
                strategy_vec[i] = std::max(0.0, regrets[i]) / regret_total;
            }
        } else {
            // Uniform strategy
            for (size_t i = 0; i < regrets.size(); i++) {
                strategy_vec[i] = 1.0 / regrets.size();
            }
        }
        return strategy_vec;
    }

std::vector<double> Trainer::get_average_strategy(size_t infoset) {
        std::vector<double> avg_strategy = strategy_sum[infoset];
        double sum = 0.0;
        
        // Calculate sum
        for (double prob : avg_strategy) {
            sum += prob;
        }
        
        // Normalize
        if (sum > 0) {
            for (size_t i = 0; i < avg_strategy.size(); i++) {
                avg_strategy[i] /= sum;
            }
        } else {
            // Uniform if no strategy recorded
            for (size_t i = 0; i < avg_strategy.size(); i++) {
                avg_strategy[i] = 1.0 / avg_strategy.size();
            }
        }
        
        return avg_strategy;
    }

double Trainer::cfr(GameState& g, double p1_reach, double p2_reach) {
        if (g.history.size() > 40) {
            std::cout << g.history.size() << " actions so far\n";
            g.print_game_state();
            std::cout << std::endl;
            return 0;
        }
        
        // std::cout << "CFR called\n";
        // g.print_game_state();
        // std::cout << std::endl;

        if (g.is_terminal()) {
            return g.get_utility();
        }
        
        int player = g.get_current_player();
        std::vector<Action> legal_actions = g.get_legal_actions();
        std::vector<Action> actions = {};

        // if 2v2, check h+a is in valid_histories
        if (g.p1_influence[0] + g.p1_influence[1] == 2 && g.p2_influence[0] + g.p2_influence[1] == 2) {
            for (Action a : legal_actions) {
                g.do_action(a);
                size_t history_hash = g.get_history_hash();
                g.undo_action();
                
                if (valid_histories.find(history_hash) != valid_histories.end()) {
                    actions.push_back(a);
                }
            }
            if (g.history.size() > 0 && (g.history[g.history.size() - 1] == CHALLENGE || g.history[g.history.size() - 2] == CHALLENGE)) {
                actions = legal_actions;
            } else {
                assert(!actions.empty());
            }
        } else {
            actions = legal_actions;
        }

        // std::cout << "Legal actions: ";
        // for (Action a : actions) {
        //     std::cout << ACTION_NAMES[a] << " ";
        // }
        // std::cout << std::endl;

        size_t infoset = g.get_infoset_hash();

        // Check for hash collision
        std::string infoset_string = g.get_infoset_string();
        if (hash_to_string.find(infoset) != hash_to_string.end()) {
            if (hash_to_string[infoset] != infoset_string) {
                assert(false && "Hash collision detected");
            }
        }
        
        // Lazy initialization
        if (regret_sum.find(infoset) == regret_sum.end()) {
            regret_sum[infoset] = std::vector<double>(actions.size(), 0.0);
            strategy_sum[infoset] = std::vector<double>(actions.size(), 0.0);
            next_actions[infoset] = actions;
            hash_to_string[infoset] = infoset_string;
        }
        
        std::vector<double> strategy = get_strategy(infoset);
        std::vector<double> action_utilities(actions.size(), 0.0);
        double node_utility = 0.0;
        
        // Recurse for each action
        for (size_t i = 0; i < actions.size(); i++) {
            g.do_action(actions[i]);
            
            double new_p1_reach = (player == 0) ? p1_reach * strategy[i] : p1_reach;
            double new_p2_reach = (player == 1) ? p2_reach * strategy[i] : p2_reach;
            
            action_utilities[i] = -cfr(g, new_p1_reach, new_p2_reach);
            
            g.undo_action();
            
            node_utility += strategy[i] * action_utilities[i];
        }
        
        // Update regrets
        double reach_prob = (player == 0) ? p2_reach : p1_reach;
        for (size_t i = 0; i < actions.size(); i++) {
            double regret = action_utilities[i] - node_utility;
            regret_sum[infoset][i] += reach_prob * regret;
            strategy_sum[infoset][i] += ((player == 0) ? p1_reach : p2_reach) * strategy[i];
        }
        
        return node_utility;
    }

double Trainer::calculate_best_response(GameState& g, const int max_player, std::array<double, NUM_HOLDINGS> pair_distribution) {
        if (g.is_terminal()) {
            return g.get_br_utility(max_player, pair_distribution);
        }

        // MAXIMIZING PLAYER TURN
        if (g.get_current_player() == max_player) {
            std::vector<Action> next_actions = g.get_legal_actions();
            double best_value = -100.0;
            // Action best_action = ASSASSINATE;
            for (size_t a = 0; a < next_actions.size(); a++) {
                g.do_action(next_actions[a]);
                double action_utility = -calculate_best_response(g, max_player, pair_distribution);
                g.undo_action();
                if (action_utility > best_value) {
                    best_value = action_utility;
                    // best_action = next_actions[a];
                }
            }
            // Print best action
            // std::cout << "Player " << max_player + 1 << ": History: ";
            // for (Action a : g.history) {
            //     std::cout << a << " ";
            // }
            // std::cout << "Best action: " << best_action << std::endl;
            return best_value;
        }
        else {
            std::array<double, NUM_ACTIONS> action_probs = {0.0};
            std::array<std::array<double, NUM_HOLDINGS>, NUM_ACTIONS> new_card_distribution = {};
            
            for (size_t h = 0; h < NUM_HOLDINGS; h++) {
                g.set_my_cards(holdings[h]);
                std::vector<Action> legal_actions = g.get_legal_actions();
                std::vector<double> avg_strategy = get_average_strategy(g.get_infoset_hash());

                for (size_t a = 0; a < legal_actions.size(); a++) {
                    int action_index = static_cast<int>(legal_actions[a]);
                    double v = avg_strategy[a] * pair_distribution[h];
                    action_probs[action_index] += v;
                    new_card_distribution[action_index][h] = v;
                }
            }

            double node_utility = 0.0;
            for (size_t a = 0; a < NUM_ACTIONS; a++) {
                if (action_probs[a] == 0) continue;
                const Action action = static_cast<Action>(a);

                // Normalize new card distribution
                std::array<double, NUM_HOLDINGS> normalized_card_dist = new_card_distribution[a];
                for (double& p : normalized_card_dist) {
                    p /= action_probs[a];
                }

                g.do_action(action);
                double action_utility = -calculate_best_response(g, max_player, normalized_card_dist);
                g.undo_action();
                node_utility += action_utility * action_probs[a];
            }
            return node_utility;
        }
        assert(false);
    }

void Trainer::train(size_t iterations) {
        std::vector<Card> card_pool = {
            ASSASSIN, ASSASSIN, ASSASSIN,
            AMBASSADOR, AMBASSADOR, AMBASSADOR,
            CAPTAIN, CAPTAIN, CAPTAIN,
            CONTESSA, CONTESSA, CONTESSA, 
            DUKE, DUKE, DUKE
        };
        std::random_device rd;
        std::mt19937 gen(rd());
        double util = 0.0;
        
        for (size_t i = 0; i < iterations; i++) {
            // Shuffle card pool
            std::shuffle(card_pool.begin(), card_pool.end(), gen);

            // Deal first two cards to players
            GameState g(RulesConfig::solver_default());
            g.set_cards(card_pool[0], card_pool[1], card_pool[2], card_pool[3]);
            
            util += cfr(g, 1.0, 1.0);

            // if (i % 10000 == 0 && i != 0) {
            //     current_utility = util / i;
            //     std::cout << "Iteration #: " << i << " EV: " << util / i << std::endl;
            //     // calculate_exploitability();
            // }
        }
        
        current_utility = util / iterations;
        std::cout << "util: " << util << ", iterations: " << iterations << std::endl;
        std::cout << "Average game value: " << util / iterations << std::endl;
    }

// Convert Card enum to index (0-4)
int Trainer::card_to_index(Card c) {
        switch(c) {
            case ASSASSIN: return 0;
            case AMBASSADOR: return 1;
            case CAPTAIN: return 2;
            case CONTESSA: return 3;
            case DUKE: return 4;
            default: return -1;
        }
    }

// Calculate pair distribution after removing 2 cards
// Returns probabilities in same order as holdings array
std::array<double, NUM_HOLDINGS> Trainer::calculate_pair_distribution(Card removed_card1, Card removed_card2) {
        std::vector<int> remaining_counts = {3, 3, 3, 3, 3};
        remaining_counts[card_to_index(removed_card1)]--;
        remaining_counts[card_to_index(removed_card2)]--;
        
        constexpr double INV_TOTAL_PAIRS = 1.0 / 78.0;  // Reciprocal of C(13, 2)
        
        std::array<double, NUM_HOLDINGS> pair_distribution;
        
        for (size_t h = 0; h < NUM_HOLDINGS; h++) {
            const int idx1 = card_to_index(holdings[h][0]);
            const int idx2 = card_to_index(holdings[h][1]);
            // Same card pair: C(count, 2) = count * (count-1) / 2
            if (idx1 == idx2) {
                const int count = remaining_counts[idx1];
                const double prob = (count >= 2) ? 
                    (count * (count - 1) * 0.5 * INV_TOTAL_PAIRS) : 0.0;
                pair_distribution[h] = prob;
            } else {
                // Different card pair: count1 * count2
                const double prob = (remaining_counts[idx1] >= 1 && remaining_counts[idx2] >= 1) ?
                    (remaining_counts[idx1] * remaining_counts[idx2] * INV_TOTAL_PAIRS) : 0.0;
                pair_distribution[h] = prob;
            }
        }
        return pair_distribution;
    }

// Calculate probability of being dealt a specific pair
double Trainer::calculate_pair_probability(Card card1, Card card2) {
        if (card1 == card2) {
            return 3.0 / 105.0; // comb(3, 2) / C(15, 2)
        } else {
            return 9.0 / 105.0; // 3 * 3 / C(15, 2)
        }
    }

void Trainer::calculate_exploitability() {
        double p1_br_utility = 0.0;
        double p2_br_utility = 0.0;
        
        // Iterate through each possible holding
        for (size_t c = 0; c < holdings.size(); c++) {
            const Card card1 = holdings[c][0];
            const Card card2 = holdings[c][1];
            std::array<double, NUM_HOLDINGS> pair_distribution = calculate_pair_distribution(card1, card2);
            
            for (int p = 0; p < 2; p++) {
                GameState g(RulesConfig::solver_default());
                g.set_cards(card1, card2, ASSASSIN, ASSASSIN);
                double v = calculate_best_response(g, p, pair_distribution) * calculate_pair_probability(card1, card2);
                if (p == 0) {
                    p1_br_utility += v;
                } else {
                    p2_br_utility -= v;
                }
            }
        }
        
        std::cout << "P1 BR EV: " << p1_br_utility << std::endl;
        std::cout << "P2 BR EV: " << p2_br_utility << std::endl;
        
        double p1_exploitability = p1_br_utility - current_utility;
        double p2_exploitability = p2_br_utility + current_utility;
        
        std::cout << "P1 exploitability: " << p1_exploitability << std::endl;
        std::cout << "P2 exploitability: " << p2_exploitability << std::endl;
    }

// FOR DEBUGGING
// Helper function to convert history to string for set storage
void Trainer::get_tree_size(size_t max_depth) {
        GameState g(RulesConfig::solver_default());
        g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
        
        std::function<size_t(GameState&, size_t)> count_histories = 
            [&](GameState& state, size_t current_depth) -> size_t {
            // Count current state
            size_t count = 1;
            static size_t test_num = 0;

            if (max_depth == current_depth) {
                if (g.is_terminal() && test_num++ % 1000000 == 0) {
                    // g.print_history();
                }
            }
            
            // Stop if we've reached max depth or terminal state
            if (current_depth >= max_depth || state.is_terminal()) {
                return count;
            }
            
            // Recurse through all legal actions
            std::vector<Action> actions = state.get_legal_actions();
            for (Action a : actions) {
                state.do_action(a);
                count += count_histories(state, current_depth + 1);
                state.undo_action();
            }
            
            return count;
        };
        
        std::cout << "MAX DEPTH: " << max_depth << " # Histories:\n" << count_histories(g, 0) << std::endl;
    }

void Trainer::get_2v2_tree_size(size_t max_depth) {
        GameState g(RulesConfig::solver_default());
        g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
        
        std::function<size_t(GameState&, size_t)> count_histories = 
            [&](GameState& state, size_t current_depth) -> size_t {
            static size_t test_num = 0;

            // Count current state
            size_t count = 1;
            
            // Stop if we've reached max depth or either player lost an influence
            if (g.p1_influence[0] + g.p1_influence[1] < 2 || g.p2_influence[0] + g.p2_influence[1] < 2) {
                
                if (max_depth == current_depth) {
                    test_num++;
                    if (test_num % 1 == 0) {
                        // g.print_history();
                    }
                }

                return count;
            }
            if (current_depth >= max_depth) {
                return count;
            }
            
            // Recurse through all legal actions
            std::vector<Action> actions = state.get_legal_actions();
            for (Action a : actions) {
                state.do_action(a);
                count += count_histories(state, current_depth + 1);
                state.undo_action();
            }
            
            return count;
        };
        
        std::cout << "MAX DEPTH: " << max_depth << " # Histories:\n" << count_histories(g, 0) << std::endl;
    }

void Trainer::collect_histories(GameState& g, std::set<std::string>& histories, 
                      size_t max_depth, bool use_original) {
        if (g.history.size() >= max_depth || g.is_terminal()) {
            histories.insert(history_to_string(g.history));
            return;
        }
        
        histories.insert(history_to_string(g.history));
        
        std::vector<Action> actions = g.get_legal_actions();
        
        for (Action a : actions) {
            g.do_action(a);
            collect_histories(g, histories, max_depth, use_original);
            g.undo_action();
        }
    }

std::string Trainer::history_to_string(const std::vector<Action>& history) {
        std::string result;
        for (Action a : history) {
            result += std::to_string(a) + ",";
        }
        return result;
    }

void Trainer::find_2v2_terminals(GameState& g, size_t move_limit) {
        // Base case: If we hit the move limit or terminal state, return
        if (g.history.size() > move_limit) {
            return;
        }

        // 2v2-ending action within the move limit -> Store the 2v2 terminal history
        if (g.history.size() > 0) {
            if (g.history[g.history.size() - 1] == CHALLENGE || (g.history[g.history.size() - 1] >= LOSE_ASSASSIN && 
                (g.history[g.history.size() - 1] <= LOSE_BOTH))) {
                // Get history hash and store the complete history
                size_t history_hash = g.get_history_hash();
                
                // Check for hash collision
                auto it = history_map.find(history_hash);
                if (it != history_map.end()) {
                    if (it->second != g.history) {
                        assert(false && "Hash collision detected\n");
                    }
                } else {
                    history_map[history_hash] = g.history;
                }
                
                valid_histories.insert(history_hash);
                return;
            }
        }

        std::vector<Action> actions = g.get_legal_actions();
        
        for (Action a : actions) {
            g.do_action(a);
            find_2v2_terminals(g, move_limit);
            g.undo_action();
        }
    }

// Helper function to initialize the search with a specific game state
std::unordered_set<size_t> Trainer::find_all_2v2_terminals(size_t move_limit) {
        std::unordered_set<size_t> terminal_histories;
        
        // First, find all terminal histories
        for (const auto& holding : holdings) {
            GameState g(RulesConfig::solver_default());
            g.set_cards(holding[0], holding[1], holding[0], holding[1]);
            find_2v2_terminals(g, move_limit);
        }

        // Store the terminal histories in a separate set
        terminal_histories = valid_histories;
        valid_histories.clear();

        // For each terminal history, generate and store all its prefixes
        for (size_t terminal_hash : terminal_histories) {
            // Get the history from our stored map
            const std::vector<Action>& history = history_map[terminal_hash];
            
            // Generate and store hashes for all prefixes
            std::vector<Action> prefix;
            prefix.reserve(history.size());
            
            for (Action a : history) {
                prefix.push_back(a);
                size_t prefix_hash = GameState::get_history_hash(prefix);
                
                // Check for hash collision on prefixes
                auto it = history_map.find(prefix_hash);
                if (it != history_map.end()) {
                    if (it->second != prefix) {
                        std::cerr << "Hash collision detected in prefix!" << std::endl;
                        std::cerr << "Existing prefix: ";
                        for (const auto& a : it->second) {
                            std::cerr << ACTION_NAMES[static_cast<int>(a)] << " ";
                        }
                        std::cerr << "\nNew prefix: ";
                        for (const auto& a : prefix) {
                            std::cerr << ACTION_NAMES[static_cast<int>(a)] << " ";
                        }
                        std::cerr << "\nHash value: " << prefix_hash << std::endl;
                        assert(false && "Hash collision detected in prefix");
                    }
                } else {
                    history_map[prefix_hash] = prefix;
                }
                
                valid_histories.insert(prefix_hash);
            }
        }

        // Add the terminal histories back
        valid_histories.insert(terminal_histories.begin(), terminal_histories.end());
        
        return valid_histories;
    }

void Trainer::compare_tree_size(size_t max_size) {
    GameState g1;
    g1.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    std::set<std::string> new_histories, original_histories;
    
    collect_histories(g1, new_histories, max_size, false);
    
    GameState g2;
    g2.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    collect_histories(g2, original_histories, max_size, true);
    
    // Find differences
    std::set<std::string> only_original;
    for (const auto& hist : original_histories) {
        if (new_histories.find(hist) == new_histories.end()) {
            only_original.insert(hist);
        }
    }
    
    std::cout << "New method: " << new_histories.size() << " histories" << std::endl;
    std::cout << "Original method: " << original_histories.size() << " histories" << std::endl;
    
    std::cout << "\n=== NEW ACTIONS LIST ONLY ===" << std::endl;
    std::cout << "Count: " << new_histories.size() << std::endl;
    for (const auto& hist : new_histories) {
        std::stringstream ss(hist);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                std::cout << ACTION_NAMES[std::stoi(token)] << " ";
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== OG ACTIONS LIST ONLY ===" << std::endl;
    std::cout << "Count: " << only_original.size() << std::endl;
    for (const auto& hist : only_original) {
        std::stringstream ss(hist);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                std::cout << ACTION_NAMES[std::stoi(token)] << " ";
            }
        }
        std::cout << std::endl;
    }
}
