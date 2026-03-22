#ifndef TRAINER_HPP
#define TRAINER_HPP

#include "game_state.hpp"

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Trainer {
public:
    std::unordered_map<size_t, std::vector<double>> regret_sum;
    std::unordered_map<size_t, std::vector<double>> strategy_sum;
    std::unordered_map<size_t, std::vector<Action>> next_actions;
    std::unordered_map<size_t, std::string>         hash_to_string;

    double current_utility;

    std::unordered_set<size_t>              valid_histories;
    std::unordered_map<size_t, std::vector<Action>> history_map;

    Trainer();

    std::vector<double> get_strategy(size_t infoset);
    std::vector<double> get_average_strategy(size_t infoset);
    double cfr(GameState& g, double p1_reach, double p2_reach);
    double calculate_best_response(GameState& g, int max_player,
                                   std::array<double, NUM_HOLDINGS> pair_distribution);
    void train(size_t iterations);

    int    card_to_index(Card c);
    std::array<double, NUM_HOLDINGS> calculate_pair_distribution(Card c1, Card c2);
    double calculate_pair_probability(Card c1, Card c2);
    void   calculate_exploitability();

    // Debugging
    void get_tree_size(size_t max_depth);
    void get_2v2_tree_size(size_t max_depth);
    void collect_histories(GameState& g, std::set<std::string>& histories,
                           size_t max_depth, bool use_original);
    std::string history_to_string(const std::vector<Action>& history);
    void find_2v2_terminals(GameState& g, size_t move_limit);
    std::unordered_set<size_t> find_all_2v2_terminals(size_t move_limit = 2);
    void compare_tree_size(size_t max_size);
};

#endif