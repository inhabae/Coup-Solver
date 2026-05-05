#ifndef COUP_SMALL_COUP_REBEL_HPP
#define COUP_SMALL_COUP_REBEL_HPP

#include "small_coup.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace small_coup::rebel {

constexpr int kDealCount = 6;

struct PublicState {
    Phase phase{Phase::TurnAction};
    int current_player{0};
    std::array<int, kPlayers> coins{kStartingCoins, kStartingCoins};
    std::array<int, kPlayers> lives{kStartingLives, kStartingLives};
    std::array<bool, kPlayers> assassinated{false, false};
    ActionMask legal_mask{0};
    std::vector<PublicEvent> history{};

    bool operator==(const PublicState& other) const;
    std::string serialize() const;
    std::vector<double> features() const;
};

struct BeliefState {
    std::array<Deal, kDealCount> deals{};
    std::array<double, kDealCount> probabilities{};

    double probability_sum() const;
};

struct SearchResult {
    std::array<double, static_cast<std::size_t>(Action::Count)> policy{};
    double value{0.0};
};

struct TrainingSample {
    PublicState public_state{};
    BeliefState belief{};
    SearchResult search_result{};
    int player{0};
    double target_value{0.0};

    std::vector<double> features() const;
};

class ValueEvaluator {
public:
    virtual ~ValueEvaluator() = default;
    virtual double evaluate(const PublicState& public_state, const BeliefState& belief, int player) const = 0;
};

class HeuristicValueEvaluator final : public ValueEvaluator {
public:
    double evaluate(const PublicState& public_state, const BeliefState& belief, int player) const override;
};

class DepthLimitedResolver {
public:
    DepthLimitedResolver(int iterations, int depth, const ValueEvaluator& evaluator, uint32_t seed = 1);

    SearchResult resolve(const GameState& root, int player);

private:
    struct LocalNode {
        InfosetKey key{};
        ActionMask legal_mask{0};
        std::array<double, static_cast<std::size_t>(Action::Count)> regret_sum{};
        std::array<double, static_cast<std::size_t>(Action::Count)> strategy_sum{};
    };

    struct LocalKeyHash {
        std::size_t operator()(InfosetKey key) const;
    };

    int iterations_{0};
    int depth_{0};
    const ValueEvaluator& evaluator_;
    std::mt19937 rng_;
    std::unordered_map<InfosetKey, LocalNode, LocalKeyHash> nodes_;

    double traversal(GameState& state, int traverser, int remaining_depth, double reach0, double reach1);
    LocalNode& node_for(const GameState& state, int player);
};

std::array<Deal, kDealCount> all_deals();
PublicState public_state_from(const GameState& state);
BeliefState belief_from_public_state(const PublicState& public_state, int known_player = -1,
                                     Card known_card = Card::None);
bool replay_public_history(const Deal& deal, const std::vector<PublicEvent>& history, GameState& out);
std::vector<double> value_features(const PublicState& public_state, const BeliefState& belief, int player);
TrainingSample make_training_sample(const GameState& state, DepthLimitedResolver& resolver,
                                    const ValueEvaluator& evaluator);
std::vector<TrainingSample> generate_training_samples(int samples, int max_steps, uint32_t seed,
                                                      int resolve_iterations, int resolve_depth);

} // namespace small_coup::rebel

#endif
