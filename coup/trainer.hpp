#ifndef COUP_TRAINER_HPP
#define COUP_TRAINER_HPP

#include "game_state.hpp"

#include <cstdint>
#include <cstddef>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace coup {

using CfrValue = float;

struct InfosetNode {
    InfosetKey key{};
    std::string debug_label{};
    ActionMask legal_mask{0};
    std::vector<CfrValue> regret_sum{};
    std::vector<CfrValue> strategy_sum{};
    std::vector<CfrValue> current_strategy{};

    InfosetNode() = default;
    InfosetNode(const InfosetKey& infoset_key, std::string label, ActionMask actions);

    std::vector<CfrValue> strategy();
    void accumulate_strategy(const std::vector<CfrValue>& strategy, CfrValue realization_weight);
    std::vector<CfrValue> average_strategy() const;
};

struct InfosetKeyHash {
    std::size_t operator()(const InfosetKey& key) const;
};

struct TrainingStats {
    int64_t iterations{0};
    CfrValue utility0_sum{0.0F};
    std::size_t infosets{0};
};

class CfrTrainer {
public:
    explicit CfrTrainer(uint32_t seed = 1, int max_public_actions = 20);
    CfrTrainer(uint32_t seed, int max_2v2_public_actions, int max_non_2v2_public_actions);

    TrainingStats train(int iterations);
    CfrValue run_iteration();

    const std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash>& nodes() const;
    void set_node_creation_callback(std::function<void(std::size_t)> callback);

private:
    uint32_t seed_{1};
    std::mt19937 rng_;
    int max_2v2_public_actions_{20};
    int max_non_2v2_public_actions_{20};
    int next_traverser_{0};
    ObservationStorePtr observation_store_;
    std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash> nodes_;
    std::function<void(std::size_t)> node_creation_callback_;

    CfrValue run_traversal(int traverser);
    CfrValue cfr(GameState& state, int traverser, CfrValue reach0, CfrValue reach1, CfrValue sample_reach);
    bool exceeds_history_limit(const GameState& state) const;
    CfrValue depth_limited_utility(const GameState& state, int traverser) const;
    InfosetNode& node_for(const GameState& state, int player);
    ChanceOutcome sample_chance(const GameState& state, int traverser, const std::vector<ChanceOutcome>& outcomes) const;
    Action sample_action(const GameState& state, int traverser, const std::vector<Action>& actions,
                         const std::vector<CfrValue>& strategy) const;
    Deal sample_deal();
};

std::vector<Action> actions_from_mask(ActionMask mask);

} // namespace coup

#endif
