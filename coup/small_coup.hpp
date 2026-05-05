#ifndef COUP_SMALL_COUP_HPP
#define COUP_SMALL_COUP_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace small_coup {

enum class Card : uint8_t {
    Assassin = 0,
    Contessa = 1,
    Civilian = 2,
    None = 3,
};

enum class Phase : uint8_t {
    TurnAction = 0,
    RespondToAssassinate = 1,
    RespondToBlock = 2,
    LoseLife = 3,
    Terminal = 4,
};

enum class Action : uint8_t {
    Income = 0,
    Assassinate = 1,
    Coup = 2,
    Allow = 3,
    BlockAssassinate = 4,
    Challenge = 5,
    LoseLife = 6,
    Count = 7,
};

using ActionMask = uint32_t;

constexpr int kPlayers = 2;
constexpr int kStartingCoins = 0;
constexpr int kStartingLives = 1;
constexpr int kAssassinateCost = 2;
constexpr int kCoupCost = 3;
constexpr int kMaxPublicHistory = 64;

struct Deal {
    std::array<Card, kPlayers> cards{Card::None, Card::None};
    Card hidden{Card::None};
};

struct InfosetKey {
    uint64_t value{0};

    bool operator==(const InfosetKey& other) const {
        return value == other.value;
    }
};

struct PublicEvent {
    Action action{Action::Count};
    int player{-1};
};

struct InfosetMetadata {
    int player{-1};
    Card private_card{Card::None};
    Phase phase{Phase::TurnAction};
    int current_player{-1};
    std::array<int, kPlayers> coins{kStartingCoins, kStartingCoins};
    std::array<int, kPlayers> lives{kStartingLives, kStartingLives};
    std::array<bool, kPlayers> assassinated{false, false};
    std::vector<PublicEvent> history{};
};

struct GameState {
    GameState();
    explicit GameState(const Deal& deal);

    void reset(const Deal& deal);
    bool is_terminal() const;
    int current_player() const;
    Phase phase() const;
    ActionMask legal_actions() const;
    bool is_legal(Action action) const;
    void apply(Action action);
    void undo();
    double utility(int player) const;
    InfosetKey infoset(int player) const;
    InfosetMetadata infoset_metadata(int player) const;
    std::string infoset_string(int player) const;

    int coins(int player) const;
    int lives(int player) const;
    bool has_assassinated(int player) const;
    Card card(int player) const;
    Card hidden_card() const;
    int public_history_size() const;
    const PublicEvent& public_event(int index) const;
    std::string debug_string() const;

private:
    struct Pending {
        Action action{Action::Count};
        int actor{-1};
        int target{-1};
        int block_actor{-1};
    };

    struct Snapshot {
        Phase phase{Phase::TurnAction};
        int current_player{0};
        std::array<int, kPlayers> coins{kStartingCoins, kStartingCoins};
        std::array<int, kPlayers> lives{kStartingLives, kStartingLives};
        std::array<bool, kPlayers> assassinated{false, false};
        std::array<Card, kPlayers> cards{Card::None, Card::None};
        Card hidden{Card::None};
        Pending pending{};
        int public_history_len{0};
    };

    Phase phase_{Phase::TurnAction};
    int current_player_{0};
    std::array<int, kPlayers> coins_{kStartingCoins, kStartingCoins};
    std::array<int, kPlayers> lives_{kStartingLives, kStartingLives};
    std::array<bool, kPlayers> assassinated_{false, false};
    std::array<Card, kPlayers> cards_{Card::None, Card::None};
    Card hidden_{Card::None};
    Pending pending_{};
    int public_history_len_{0};
    std::array<PublicEvent, kMaxPublicHistory> public_history_{};
    std::vector<Snapshot> undo_stack_{};

    Snapshot snapshot() const;
    void restore(const Snapshot& snapshot);
    void append_event(Action action, int player);
    void end_turn(int next_player);
    int opponent(int player) const;
};

struct InfosetNode {
    InfosetKey key{};
    InfosetMetadata metadata{};
    std::string label{};
    ActionMask legal_mask{0};
    std::vector<double> regret_sum{};
    std::vector<double> strategy_sum{};
    std::vector<double> current_strategy{};

    InfosetNode() = default;
    InfosetNode(InfosetKey infoset_key, InfosetMetadata infoset_metadata, std::string infoset_label,
                ActionMask actions);

    std::vector<double> strategy();
    void accumulate_strategy(const std::vector<double>& strategy, double realization_weight);
    std::vector<double> average_strategy() const;
};

struct InfosetKeyHash {
    std::size_t operator()(InfosetKey key) const;
};

struct TrainingStats {
    int iterations{0};
    double utility0_sum{0.0};
    std::size_t infosets{0};
};

class CfrTrainer {
public:
    explicit CfrTrainer(uint32_t seed = 1, int max_public_actions = 16);

    TrainingStats train(int iterations);
    const std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash>& nodes() const;

private:
    std::mt19937 rng_;
    int max_public_actions_{16};
    std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash> nodes_;

    double run_traversal(int traverser, const Deal& deal);
    double cfr(GameState& state, int traverser, double reach0, double reach1);
    double depth_limited_utility(const GameState& state, int traverser) const;
    InfosetNode& node_for(const GameState& state, int player);
    Deal sample_deal();
};

ActionMask action_bit(Action action);
std::vector<Action> actions_from_mask(ActionMask mask);
const char* card_name(Card card);
const char* action_name(Action action);
const char* phase_name(Phase phase);

} // namespace small_coup

#endif
