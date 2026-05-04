#include "trainer.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace coup {

namespace {

constexpr CfrValue kDefaultStrategyEpsilon = 1e-7F;
constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

int action_index(Action action) {
    return static_cast<int>(action);
}

uint64_t splitmix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

uint64_t mix_u64(uint64_t seed, uint64_t value) {
    return splitmix64(seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)));
}

double unit_interval(uint64_t value) {
    constexpr double denom = static_cast<double>(uint64_t{1} << 53);
    return static_cast<double>(value >> 11) / denom;
}

} // namespace

std::vector<Action> actions_from_mask(ActionMask mask) {
    std::vector<Action> actions;
    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        if ((mask & action_bit(action)) != 0) actions.push_back(action);
    }
    return actions;
}

InfosetNode::InfosetNode(const InfosetKey& infoset_key, ActionMask actions)
    : key(infoset_key),
      legal_mask(actions) {
    regret_sum.fill(0.0F);
    strategy_sum.fill(0.0F);
}

std::array<CfrValue, static_cast<std::size_t>(Action::Count)> InfosetNode::strategy() const {
    const std::vector<Action> legal_actions = actions_from_mask(legal_mask);
    if (legal_actions.empty()) throw std::runtime_error("infoset has no legal actions");

    std::array<CfrValue, kActionCount> current{};
    current.fill(0.0F);

    CfrValue normalizer = 0.0F;
    for (Action action : legal_actions) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        current[idx] = std::max(0.0F, regret_sum[idx]);
        normalizer += current[idx];
    }

    const CfrValue default_probability = 1.0F / static_cast<CfrValue>(legal_actions.size());
    for (Action action : legal_actions) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        current[idx] = normalizer > kDefaultStrategyEpsilon
            ? current[idx] / normalizer
            : default_probability;
    }

    return current;
}

void InfosetNode::accumulate_strategy(
    const std::array<CfrValue, kActionCount>& strat,
    CfrValue realization_weight) {
    for (Action action : actions_from_mask(legal_mask)) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        strategy_sum[idx] += realization_weight * strat[idx];
    }
}

std::array<CfrValue, static_cast<std::size_t>(Action::Count)> InfosetNode::average_strategy() const {
    std::array<CfrValue, kActionCount> average{};
    average.fill(0.0F);
    const std::vector<Action> legal_actions = actions_from_mask(legal_mask);
    if (legal_actions.empty()) return average;

    CfrValue normalizer = 0.0F;
    for (Action action : legal_actions) {
        normalizer += strategy_sum[static_cast<std::size_t>(action_index(action))];
    }

    const CfrValue default_probability = 1.0F / static_cast<CfrValue>(legal_actions.size());
    for (Action action : legal_actions) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        average[idx] = normalizer > kDefaultStrategyEpsilon
            ? strategy_sum[idx] / normalizer
            : default_probability;
    }
    return average;
}

std::size_t InfosetKeyHash::operator()(const InfosetKey& key) const {
    std::size_t seed = key.public_obs_id;
    seed ^= static_cast<std::size_t>(key.private_obs_id) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

CfrTrainer::CfrTrainer(uint32_t seed, int max_public_actions)
    : CfrTrainer(seed, max_public_actions, max_public_actions) {}

CfrTrainer::CfrTrainer(uint32_t seed, int max_2v2_public_actions, int max_non_2v2_public_actions)
    : seed_(seed),
      rng_(seed),
      max_2v2_public_actions_(max_2v2_public_actions),
      max_non_2v2_public_actions_(max_non_2v2_public_actions),
      observation_store_(make_observation_store()) {
    if (max_2v2_public_actions_ <= 0 || max_2v2_public_actions_ >= kMaxPublicHistory) {
        throw std::invalid_argument("max 2v2 public actions must be between 1 and kMaxPublicHistory - 1");
    }
    if (max_non_2v2_public_actions_ <= 0 || max_non_2v2_public_actions_ >= kMaxPublicHistory) {
        throw std::invalid_argument("max non-2v2 public actions must be between 1 and kMaxPublicHistory - 1");
    }
    if (max_2v2_public_actions_ + max_non_2v2_public_actions_ > kMaxPublicHistory) {
        throw std::invalid_argument("combined public action limits must fit in kMaxPublicHistory");
    }
}

TrainingStats CfrTrainer::train(int iterations) {
    if (iterations < 0) throw std::invalid_argument("iterations must be non-negative");

    TrainingStats stats;
    for (int i = 0; i < iterations; ++i) {
        const int traverser = i % kPlayers;
        const CfrValue utility = run_traversal(traverser);
        if (traverser == 0) {
            stats.utility0_sum += utility;
        } else {
            stats.utility0_sum -= utility;
        }
        ++stats.iterations;
    }
    stats.infosets = nodes_.size();
    return stats;
}

CfrValue CfrTrainer::run_iteration() {
    const int traverser = next_traverser_;
    next_traverser_ = 1 - next_traverser_;
    return run_traversal(traverser);
}

CfrValue CfrTrainer::run_traversal(int traverser) {
    assert(traverser >= 0 && traverser < kPlayers);
    const Deal deal = sample_deal();
    GameState state(deal, observation_store_);
    return cfr(state, traverser, 1.0F, 1.0F, 1.0F);
}

const std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash>& CfrTrainer::nodes() const {
    return nodes_;
}

const ObservationStore& CfrTrainer::observation_store() const {
    return *observation_store_;
}

void CfrTrainer::set_node_creation_callback(std::function<void(std::size_t)> callback) {
    node_creation_callback_ = std::move(callback);
}

CfrValue CfrTrainer::cfr(GameState& state, int traverser, CfrValue reach0, CfrValue reach1, CfrValue sample_reach) {
    if (state.is_terminal()) return static_cast<CfrValue>(state.utility(traverser));
    if (exceeds_history_limit(state)) {
        return depth_limited_utility(state, traverser);
    }

    if (state.is_chance_node()) {
        const std::vector<ChanceOutcome> outcomes = state.chance_outcomes();
        const ChanceOutcome outcome = sample_chance(state, traverser, outcomes);
        state.apply_chance(outcome);
        const CfrValue value = cfr(state, traverser, reach0, reach1,
                                   sample_reach * static_cast<CfrValue>(outcome.probability));
        state.undo_chance();
        return value;
    }

    const int player = state.current_player();
    InfosetNode& node = node_for(state, player);
    const CfrValue player_reach = player == 0 ? reach0 : reach1;
    const auto strategy = node.strategy();
    if (player == traverser) {
        node.accumulate_strategy(strategy, player_reach / sample_reach);
    }
    const std::vector<Action> legal_actions = actions_from_mask(state.legal_actions());

    if (player != traverser) {
        const Action action = sample_action(state, traverser, legal_actions, strategy);
        const int index = action_index(action);
        const CfrValue probability = strategy[static_cast<std::size_t>(index)];
        state.apply(action);
        const CfrValue value = player == 0
            ? cfr(state, traverser, reach0 * probability, reach1, sample_reach * probability)
            : cfr(state, traverser, reach0, reach1 * probability, sample_reach * probability);
        state.undo();
        return value;
    }

    CfrValue node_value = 0.0F;
    std::array<CfrValue, kActionCount> action_values{};
    action_values.fill(0.0F);
    for (Action action : legal_actions) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        const CfrValue probability = strategy[idx];
        state.apply(action);
        action_values[idx] = player == 0
            ? cfr(state, traverser, reach0 * probability, reach1, sample_reach)
            : cfr(state, traverser, reach0, reach1 * probability, sample_reach);
        state.undo();
        node_value += probability * action_values[idx];
    }

    for (Action action : legal_actions) {
        const std::size_t idx = static_cast<std::size_t>(action_index(action));
        node.regret_sum[idx] += action_values[idx] - node_value;
    }

    return node_value;
}

bool CfrTrainer::exceeds_history_limit(const GameState& state) const {
    if (state.is_2v2()) {
        return state.public_history_size() >= max_2v2_public_actions_;
    }
    return state.post_2v2_public_history_size() >= max_non_2v2_public_actions_;
}

CfrValue CfrTrainer::depth_limited_utility(const GameState& state, int traverser) const {
    const int opponent = 1 - traverser;
    CfrValue value = 0.5F * static_cast<CfrValue>((state.live(traverser, 0) ? 1 : 0) +
                                                  (state.live(traverser, 1) ? 1 : 0) -
                                                  (state.live(opponent, 0) ? 1 : 0) -
                                                  (state.live(opponent, 1) ? 1 : 0));
    value += 0.05F * static_cast<CfrValue>(state.coins(traverser) - state.coins(opponent));
    return std::max(-1.0F, std::min(1.0F, value));
}

InfosetNode& CfrTrainer::node_for(const GameState& state, int player) {
    const InfosetKey key = state.infoset(player);
    const ActionMask legal_mask = state.legal_actions();
    // No debug_label stored — pass only key and legal_mask.
    auto [it, inserted] = nodes_.try_emplace(key, key, legal_mask);
    if (inserted && node_creation_callback_) {
        node_creation_callback_(nodes_.size());
    }
    if (!inserted && it->second.legal_mask != legal_mask) {
        throw std::runtime_error("infoset key collision or inconsistent legal action set");
    }
    return it->second;
}

ChanceOutcome CfrTrainer::sample_chance(const GameState& state, int traverser,
                                        const std::vector<ChanceOutcome>& outcomes) const {
    if (outcomes.empty()) throw std::runtime_error("chance node has no outcomes");
    uint64_t hash = seed_;
    hash = mix_u64(hash, 0x4348414e4345ULL);
    hash = mix_u64(hash, static_cast<uint64_t>(traverser));
    hash = mix_u64(hash, static_cast<uint64_t>(state.public_observation_id()));
    hash = mix_u64(hash, static_cast<uint64_t>(state.private_observation_id(0)));
    hash = mix_u64(hash, static_cast<uint64_t>(state.private_observation_id(1)));
    hash = mix_u64(hash, static_cast<uint64_t>(state.public_history_size()));
    hash = mix_u64(hash, static_cast<uint64_t>(state.current_player()));
    hash = mix_u64(hash, static_cast<uint64_t>(state.phase()));
    double threshold = unit_interval(hash);
    for (const ChanceOutcome& outcome : outcomes) {
        threshold -= outcome.probability;
        if (threshold <= 0.0) return outcome;
    }
    return outcomes.back();
}

Action CfrTrainer::sample_action(const GameState& state, int traverser, const std::vector<Action>& actions,
                                 const std::array<CfrValue, static_cast<std::size_t>(Action::Count)>& strategy) const {
    if (actions.empty()) throw std::runtime_error("cannot sample from empty action set");
    const int player = state.current_player();
    const InfosetKey key = state.infoset(player);
    uint64_t hash = seed_;
    hash = mix_u64(hash, 0x414354494f4eULL);
    hash = mix_u64(hash, static_cast<uint64_t>(traverser));
    hash = mix_u64(hash, static_cast<uint64_t>(player));
    hash = mix_u64(hash, static_cast<uint64_t>(key.public_obs_id));
    hash = mix_u64(hash, static_cast<uint64_t>(key.private_obs_id));
    hash = mix_u64(hash, static_cast<uint64_t>(state.public_history_size()));
    double threshold = unit_interval(hash);
    for (Action action : actions) {
        threshold -= strategy[static_cast<std::size_t>(action_index(action))];
        if (threshold <= 0.0) return action;
    }
    return actions.back();
}

Deal CfrTrainer::sample_deal() {
    std::vector<Card> deck;
    deck.reserve(15);
    for (int c = 0; c < kCardTypes; ++c) {
        for (int copy = 0; copy < 3; ++copy) deck.push_back(static_cast<Card>(c));
    }
    std::shuffle(deck.begin(), deck.end(), rng_);

    Deal deal;
    deal.cards = {{{{deck[0], deck[1]}}, {{deck[2], deck[3]}}}};
    return deal;
}

} // namespace coup