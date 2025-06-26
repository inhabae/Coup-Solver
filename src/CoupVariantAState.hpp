#ifndef COUP_VARIANT_A_STATE_HPP
#define COUP_VARIANT_A_STATE_HPP

#include <vector>
#include <memory>
#include <string>

using Player = int;

enum Action {
    GAME_START,
    INCOME,
    TAX,
    ASSASSINATE,
    COUP,
    BLOCK_ASSASSINATE,
    CHALLENGE,
    PASS_BLOCK,
    LOSE_CARD,
    SHOW_CARD
};

enum Card {
    INVALID_CARD,
    ASSASSIN,
    CONTESSA,
    DUKE
};

class CoupVariantAState{
public:
    // Constructor
    CoupVariantAState();

    // Implementations of pure virtual methods from GameState
    bool is_terminal() const;

    bool is_chance_node() const;

    Player get_current_player() const;

    std::vector<Action> get_legal_actions() const;

    std::unique_ptr<CoupVariantAState> get_child(Action action) const;

    std::string get_information_set_key() const;

    std::vector<double> get_utilities() const;

// private:
    Player current_player;
    Card player1_card;
    Card player2_card;
    int player1_coins;
    int player2_coins;
    bool did_assassinate_p1;
    bool did_assassinate_p2;

    std::vector<Action> action_history;
};

#endif // COUP_VARIANT_A_STATE_HPP
