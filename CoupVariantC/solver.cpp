#include "game_state.hpp"

#include <iostream>

class Solver {
public:
    std::unordered_map<size_t, std::vector<double>> regret_sum;
    std::unordered_map<size_t, std::vector<double>> strategy_sum;
    std::unordered_map<size_t, std::vector<double>> strategy;
    std::unordered_map<size_t, std::vector<Action>> next_actions;
    std::unordered_map<std::string, size_t> string_to_hash;

    Solver() {}

    void setup(GameState g) {
        std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};
        for (int i = 0; i < cards.size(); i++) {
            g.set_cards(cards[i], cards[i]);
            
            std::string infoset_string = std::to_string(cards[i]) + g.action_history;
            size_t hash = get_hash(infoset_string);
            
            std::vector<Action> legal_actions = g.get_legal_actions();
            next_actions[hash] = legal_actions;
            regret_sum[hash] = std::vector<double>(legal_actions.size(), 0.0);
            strategy[hash] = std::vector<double>(legal_actions.size(), 0.0);
            strategy_sum[hash] = std::vector<double>(legal_actions.size(), 0.0);
            
            for (int j = 0; j < legal_actions.size(); j++) {
                GameState new_state = g;
                new_state.apply_action(legal_actions[j]);
                setup(new_state);
            }
        }
    }
};

int main() {
    GameState g = GameState();
    g.is_terminal();
    Solver solver = Solver();
    solver.setup();
    return 0;
}