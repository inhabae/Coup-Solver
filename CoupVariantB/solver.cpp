#include "game_state.hpp"

#include <iostream>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <cassert>


class Solver {
public:
    std::unordered_map<size_t, std::vector<double>> regret_sum;
    std::unordered_map<size_t, std::vector<double>> strategy_sum;
    std::unordered_map<size_t, std::vector<Action>> next_actions;
    std::unordered_map<size_t, std::string> hash_to_string;
    
    double current_utility = 0.0;

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

    double calculate_best_response(GameState& g, const int max_player, const std::vector<Card> opp_cards, std::vector<double> card_distribution) {
        if (g.is_terminal()) {
            return g.get_br_utility(max_player, card_distribution);
        }

        std::vector<Action> next_actions = g.get_legal_actions();

        if (g.get_current_player() == max_player) {
            double best_value = -100.0;
            // Action best_action = ASSASSINATE;
            for (int a = 0; a < next_actions.size(); a++) {
                g.apply_action(next_actions[a]);
                double action_utility = -calculate_best_response(g, max_player, opp_cards, card_distribution);
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
            std::vector<double> action_probs(next_actions.size(), 0.0);
            std::vector<std::vector<double>> new_card_distribution(next_actions.size());
            for (int a = 0; a < next_actions.size(); a++) {
                new_card_distribution[a].resize(opp_cards.size());
                for (int c = 0; c < opp_cards.size(); c++) {
                    g.set_my_card(opp_cards[c]);
                    size_t infoset = g.get_hash();
                    std::vector<double> avg_strategy = get_average_strategy(infoset);
                    double v = avg_strategy[a] * card_distribution[c]; // NOTE: Without training avg_strategy might not exist, leading to SegFault
                    action_probs[a] += v;
                    new_card_distribution[a][c] = v;
                }
            }
            
            double node_utility = 0.0;
            for (int a = 0; a < next_actions.size(); a++) {
                g.apply_action(next_actions[a]);

                // Normalize new card distribution
                std::vector<double> normalized_card_dist = new_card_distribution[a];
                if (action_probs[a] > 0) {
                    for (double& p : normalized_card_dist) {
                        p /= action_probs[a];
                    }
                }

                double action_utility = -calculate_best_response(g, max_player, opp_cards, normalized_card_dist);
                g.undo_action();
                node_utility += action_utility * action_probs[a];
            }
            return node_utility;
        }
        assert(false);
    }

    void train(int iterations) {
        std::vector<Card> card_pool = {
            ASSASSIN, ASSASSIN, 
            CAPTAIN, CAPTAIN, CAPTAIN, CAPTAIN, 
            CONTESSA, CONTESSA, 
            DUKE, DUKE
        };
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
        
        current_utility = util / iterations;
        std::cout << "Average game value: " << util / iterations << std::endl;
    }

    void calculate_exploitability() {
        // For each card
        double p1_br_utility = 0.0;
        double p2_br_utility = 0.0;
        std::vector<Card> cards = {ASSASSIN, CAPTAIN, CONTESSA, DUKE};
        std::vector<int> card_nums = {2, 4, 2, 2}; // 2 Assassins, 4 Captains, 2 Contesssas, 2 Dukes
        for (int c = 0; c < cards.size(); c++) {
            std::vector<double> card_distribution;
            // Calculate card distribution
            for (int i = 0; i < cards.size(); i++) {
                int card_num = card_nums[i];
                if (i == c) card_num -= 1;
                card_distribution.push_back(card_num / 9.0);
            }   

            for (int p = 0; p < 2; p++) {
                GameState g;
                g.set_cards(cards[c], cards[c]);
                if (p == 0) {
                    double v = calculate_best_response(g, p, cards, card_distribution) * (card_nums[c] / 10.0);
                    p1_br_utility += v;
                } else {
                    double v =  -1 * calculate_best_response(g, p, cards, card_distribution) * (card_nums[c] / 10.0);
                    p2_br_utility += v;
                }
            }
        }
        std::cout << "P1 BR EV: " << p1_br_utility << std::endl;
        std::cout << "P2 BR EV: " << p2_br_utility << std::endl;


        double p1_exploitability = (p1_br_utility - current_utility);
        double p2_exploitability = (p2_br_utility + current_utility);

        std::cout << "P1 exploitability: " << p1_exploitability << std::endl;
        std::cout << "P2 exploitability: " << p2_exploitability << std::endl;
    }
};

int main() {
    Solver solver = Solver();
    solver.train(10000000);
    solver.calculate_exploitability();
    return 0;
}