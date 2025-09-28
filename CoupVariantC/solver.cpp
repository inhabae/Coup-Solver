#include "game_state.hpp"

#include <iostream>
#include <unordered_map>
#include <random>
#include <algorithm>


class Solver {
public:
    std::unordered_map<size_t, std::vector<double>> regret_sum;
    std::unordered_map<size_t, std::vector<double>> strategy_sum;
    std::unordered_map<size_t, std::vector<Action>> next_actions;
    std::unordered_map<size_t, std::string> hash_to_string;

    Solver() {}

    std::vector<double> get_strategy(size_t infoset) {
        const std::vector<double>& regrets = regret_sum[infoset];
        std::vector<double> strategy_vec(regrets.size(), 0.0);
        double regret_total = 0.0;
        for (int i = 0; i < regrets.size(); i++) {
            if (regrets[i] > 0) {
                regret_total += regrets[i];
            }
        }
        if (regret_total > 0) {
            for (int i = 0; i < regrets.size(); i++) {
                strategy_vec[i] = std::max(0.0, regrets[i]) / regret_total;
            }
        } else {
            // Uniform strategy
            for (int i = 0; i < regrets.size(); i++) {
                strategy_vec[i] = 1.0 / regrets.size();
            }
        }
        return strategy_vec;
    }

    std::vector<double> get_average_strategy(size_t infoset) {
        std::vector<double> avg_strategy = strategy_sum[infoset];
        double sum = 0.0;
        
        // Calculate sum
        for (double prob : avg_strategy) {
            sum += prob;
        }
        
        // Normalize
        if (sum > 0) {
            for (int i = 0; i < avg_strategy.size(); i++) {
                avg_strategy[i] /= sum;
            }
        } else {
            // Uniform if no strategy recorded
            for (int i = 0; i < avg_strategy.size(); i++) {
                avg_strategy[i] = 1.0 / avg_strategy.size();
            }
        }
        
        return avg_strategy;
    }

    double cfr(GameState& g, double p1_reach, double p2_reach) {
        if (g.is_terminal()) {
            return g.get_utility();
        }
        
        int player = g.get_current_player();
        std::vector<Action> actions = g.get_legal_actions();
        
        size_t infoset = g.get_hash();
        
        // Lazy initialization
        if (regret_sum.find(infoset) == regret_sum.end()) {
            regret_sum[infoset] = std::vector<double>(actions.size(), 0.0);
            strategy_sum[infoset] = std::vector<double>(actions.size(), 0.0);
            next_actions[infoset] = actions;
            hash_to_string[infoset] = g.get_infoset_string();
        }
        
        std::vector<double> strategy = get_strategy(infoset);
        std::vector<double> action_utilities(actions.size(), 0.0);
        double node_utility = 0.0;
        
        // Recurse for each action
        for (int i = 0; i < actions.size(); i++) {
            g.apply_action(actions[i]);
            
            double new_p1_reach = (player == 0) ? p1_reach * strategy[i] : p1_reach;
            double new_p2_reach = (player == 1) ? p2_reach * strategy[i] : p2_reach;
            
            action_utilities[i] = -cfr(g, new_p1_reach, new_p2_reach);
            
            g.undo_action();
            
            node_utility += strategy[i] * action_utilities[i];
        }
        
        // Update regrets
        double reach_prob = (player == 0) ? p2_reach : p1_reach;
        for (int i = 0; i < actions.size(); i++) {
            double regret = action_utilities[i] - node_utility;
            regret_sum[infoset][i] += reach_prob * regret;
            strategy_sum[infoset][i] += ((player == 0) ? p1_reach : p2_reach) * strategy[i];
        }
        
        return node_utility;
    }

    void train(int iterations) {
        std::vector<Card> card_pool = {ASSASSIN, ASSASSIN, DUKE, DUKE, CONTESSA, CONTESSA};
        std::random_device rd;
        std::mt19937 gen(rd());
        double util = 0.0;
        
        for (int i = 0; i < iterations; i++) {
            // Shuffle card pool
            std::shuffle(card_pool.begin(), card_pool.end(), gen);
            
            // Deal first two cards to players
            GameState g;
            g.set_cards(card_pool[0], card_pool[1]);
            
            util += cfr(g, 1.0, 1.0);
        }
        
        std::cout << "Average game value: " << util / iterations << std::endl;
        
        // Print strategies sorted by infoset string length
        std::vector<std::pair<size_t, std::string>> sorted_infosets;

        // // Collect all infosets with their strings
        // for (const auto& pair : regret_sum) {
        //     size_t infoset = pair.first;
        //     if (hash_to_string.find(infoset) != hash_to_string.end()) {
        //         sorted_infosets.push_back({infoset, hash_to_string[infoset]});
        //     }
        // }

        // // Sort by string length
        // std::sort(sorted_infosets.begin(), sorted_infosets.end(), 
        //     [](const auto& a, const auto& b) {
        //         return a.second.length() < b.second.length();
        //     });

        // // Print sorted strategies
        // for (const auto& pair : sorted_infosets) {
        //     size_t infoset = pair.first;
        //     std::vector<double> avg_strategy = get_average_strategy(infoset);
        //     std::cout << "Infoset " << pair.second << ": ";
        //     for (double prob : avg_strategy) {
        //         std::cout << prob << " ";
        //     }
        //     std::cout << std::endl;
        // }
    }
};

int main() {
    Solver solver = Solver();
    solver.train(1000000);
    return 0;
}