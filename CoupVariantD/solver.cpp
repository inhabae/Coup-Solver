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

    std::vector<double> get_average_strategy(size_t infoset) {
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
        for (size_t i = 0; i < actions.size(); i++) {
            g.apply_action(actions[i]);
            
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

    // double calculate_best_response(GameState& g, const int max_player, const std::vector<Card> opp_cards, std::vector<double> card_distribution) {
    //     if (g.is_terminal()) {
    //         return g.get_br_utility(max_player, card_distribution);
    //     }

    //     std::vector<Action> next_actions = g.get_legal_actions();

    //     if (g.get_current_player() == max_player) {
    //         double best_value = -100.0;
    //         // Action best_action = ASSASSINATE;
    //         for (size_t a = 0; a < next_actions.size(); a++) {
    //             g.apply_action(next_actions[a]);
    //             double action_utility = -calculate_best_response(g, max_player, opp_cards, card_distribution);
    //             g.undo_action();
    //             if (action_utility > best_value) {
    //                 best_value = action_utility;
    //                 // best_action = next_actions[a];
    //             }
    //         }
    //         // Print best action
    //         // std::cout << "Player " << max_player + 1 << ": History: ";
    //         // for (Action a : g.history) {
    //         //     std::cout << a << " ";
    //         // }
    //         // std::cout << "Best action: " << best_action << std::endl;
    //         return best_value;
    //     }
    //     else {
    //         std::vector<double> action_probs(next_actions.size(), 0.0);
    //         std::vector<std::vector<double>> new_card_distribution(next_actions.size());
    //         for (size_t a = 0; a < next_actions.size(); a++) {
    //             new_card_distribution[a].resize(opp_cards.size());
    //             for (size_t c = 0; c < opp_cards.size(); c++) {
    //                 g.set_my_cards(opp_cards[c]);
    //                 size_t infoset = g.get_hash();
    //                 std::vector<double> avg_strategy = get_average_strategy(infoset);
    //                 double v = avg_strategy[a] * card_distribution[c]; // NOTE: Without training avg_strategy might not exist, leading to SegFault
    //                 action_probs[a] += v;
    //                 new_card_distribution[a][c] = v;
    //             }
    //         }
            
    //         double node_utility = 0.0;
    //         for (size_t a = 0; a < next_actions.size(); a++) {
    //             g.apply_action(next_actions[a]);

    //             // Normalize new card distribution
    //             std::vector<double> normalized_card_dist = new_card_distribution[a];
    //             if (action_probs[a] > 0) {
    //                 for (double& p : normalized_card_dist) {
    //                     p /= action_probs[a];
    //                 }
    //             }
    //             double action_utility = -calculate_best_response(g, max_player, opp_cards, normalized_card_dist);
    //             g.undo_action();
    //             node_utility += action_utility * action_probs[a];
    //         }
    //         return node_utility;
    //     }
    //     assert(false);
    // }

    void train(size_t iterations) {
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
            GameState g;
            g.set_cards(card_pool[0], card_pool[1], card_pool[2], card_pool[3]);
            
            util += cfr(g, 1.0, 1.0);

            if (i % 10000 == 0 && i != 0) {
                current_utility = util / i;
                std::cout << "Iteration #: " << i << " EV: " << util / i << std::endl;
                calculate_exploitability();
            }
        }
        
        current_utility = util / iterations;
        std::cout << "Average game value: " << util / iterations << std::endl;
    }

    // Convert Card enum to index (0-4)
    int card_to_index(Card c) {
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
    std::vector<double> calculate_pair_distribution(Card removed_card1, Card removed_card2) {
        std::vector<int> remaining_counts = {3, 3, 3, 3, 3};
        remaining_counts[card_to_index(removed_card1)]--;
        remaining_counts[card_to_index(removed_card2)]--;
        
        constexpr double INV_TOTAL_PAIRS = 1.0 / 78.0;  // Reciprocal of C(13, 2)
        
        std::vector<double> pair_distribution;
        pair_distribution.reserve(holdings.size());
        
        // Iterate through holdings array order to match exactly
        for (const auto& holding : holdings) {
            const int idx1 = card_to_index(holding[0]);
            const int idx2 = card_to_index(holding[1]);
            
            if (holding[0] == holding[1]) {
                // Same card pair: C(count, 2) = count * (count-1) / 2
                const int count = remaining_counts[idx1];
                const double prob = (count >= 2) ? 
                    (count * (count - 1) * 0.5 * INV_TOTAL_PAIRS) : 0.0;
                pair_distribution.push_back(prob);
            } else {
                // Different card pair: count1 * count2
                const double prob = (remaining_counts[idx1] >= 1 && remaining_counts[idx2] >= 1) ?
                    (remaining_counts[idx1] * remaining_counts[idx2] * INV_TOTAL_PAIRS) : 0.0;
                pair_distribution.push_back(prob);
            }
        }
        return pair_distribution;
    }

    // Calculate probability of being dealt a specific pair
    double calculate_pair_probability(Card card1, Card card2) {
        if (card1 == card2) {
            return 3.0 / 105.0; // comb(3, 2) / C(15, 2)
        } else {
            return 9.0 / 105.0; // 3 * 3 / C(15, 2)
        }
    }

    void calculate_exploitability() {
        double p1_br_utility = 0.0;
        double p2_br_utility = 0.0;
        
        // Iterate through each possible holding
        for (size_t c = 0; c < holdings.size(); c++) {
            const Card card1 = holdings[c][0];
            const Card card2 = holdings[c][1];
            std::vector<double> pair_distribution = calculate_pair_distribution(card1, card2);
            
            for (int p = 0; p < 2; p++) {
                GameState g;
                g.set_cards(card1, card2, ASSASSIN, ASSASSIN);
                
                double v = 0; 
                // double v = calculate_best_response(g, p, pair_distribution) * calculate_pair_probability(card1, card2);
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
};

int main() {
    Solver solver = Solver();
    // solver.train(10000000);
    solver.calculate_exploitability();
    return 0;
}