#include "small_coup_rebel.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace small_coup::rebel {

namespace {

constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

int action_index(Action action) {
    return static_cast<int>(action);
}

bool same_public_state(const PublicState& lhs, const PublicState& rhs) {
    if (lhs.phase != rhs.phase ||
        lhs.current_player != rhs.current_player ||
        lhs.coins != rhs.coins ||
        lhs.lives != rhs.lives ||
        lhs.assassinated != rhs.assassinated ||
        lhs.history.size() != rhs.history.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.history.size(); ++i) {
        if (lhs.history[i].player != rhs.history[i].player ||
            lhs.history[i].action != rhs.history[i].action) {
            return false;
        }
    }
    return true;
}

std::array<double, kActionCount> regret_matching(ActionMask legal_mask,
                                                 const std::array<double, kActionCount>& regret_sum) {
    std::array<double, kActionCount> strategy{};
    double normalizer = 0.0;
    const std::vector<Action> actions = actions_from_mask(legal_mask);
    for (Action action : actions) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        strategy[index] = std::max(0.0, regret_sum[index]);
        normalizer += strategy[index];
    }
    const double fallback = actions.empty() ? 0.0 : 1.0 / static_cast<double>(actions.size());
    for (Action action : actions) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        strategy[index] = normalizer > 1e-12 ? strategy[index] / normalizer : fallback;
    }
    return strategy;
}

std::array<double, kActionCount> average_strategy(ActionMask legal_mask,
                                                  const std::array<double, kActionCount>& strategy_sum) {
    std::array<double, kActionCount> strategy{};
    double normalizer = 0.0;
    const std::vector<Action> actions = actions_from_mask(legal_mask);
    for (Action action : actions) normalizer += strategy_sum[static_cast<std::size_t>(action_index(action))];
    const double fallback = actions.empty() ? 0.0 : 1.0 / static_cast<double>(actions.size());
    for (Action action : actions) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        strategy[index] = normalizer > 1e-12 ? strategy_sum[index] / normalizer : fallback;
    }
    return strategy;
}

double heuristic_from_public(const PublicState& public_state, int player) {
    const int other = 1 - player;
    if (public_state.phase == Phase::Terminal) {
        if (public_state.lives[player] > 0 && public_state.lives[other] <= 0) return 1.0;
        if (public_state.lives[player] <= 0 && public_state.lives[other] > 0) return -1.0;
        return 0.0;
    }
    double value = 0.65 * static_cast<double>(public_state.lives[player] - public_state.lives[other]);
    value += 0.12 * static_cast<double>(public_state.coins[player] - public_state.coins[other]);
    if (!public_state.assassinated[player]) value += 0.04;
    if (!public_state.assassinated[other]) value -= 0.04;
    return std::max(-1.0, std::min(1.0, value));
}

} // namespace

bool PublicState::operator==(const PublicState& other) const {
    return same_public_state(*this, other) && legal_mask == other.legal_mask;
}

std::string PublicState::serialize() const {
    std::ostringstream out;
    out << static_cast<int>(phase) << "|" << current_player
        << "|c" << coins[0] << "," << coins[1]
        << "|l" << lives[0] << "," << lives[1]
        << "|a" << (assassinated[0] ? 1 : 0) << "," << (assassinated[1] ? 1 : 0)
        << "|m" << legal_mask
        << "|h";
    for (const PublicEvent& event : history) {
        out << "/" << event.player << ":" << static_cast<int>(event.action);
    }
    return out.str();
}

std::vector<double> PublicState::features() const {
    std::vector<double> out;
    out.reserve(5 + kPlayers * 3 + static_cast<int>(Action::Count) + 16);
    out.push_back(static_cast<double>(static_cast<int>(phase)));
    out.push_back(static_cast<double>(current_player));
    for (int player = 0; player < kPlayers; ++player) out.push_back(static_cast<double>(coins[player]) / 3.0);
    for (int player = 0; player < kPlayers; ++player) out.push_back(static_cast<double>(lives[player]));
    for (int player = 0; player < kPlayers; ++player) out.push_back(assassinated[player] ? 1.0 : 0.0);
    for (int action = 0; action < static_cast<int>(Action::Count); ++action) {
        out.push_back((legal_mask & action_bit(static_cast<Action>(action))) != 0 ? 1.0 : 0.0);
    }
    out.push_back(static_cast<double>(history.size()) / 16.0);
    return out;
}

double BeliefState::probability_sum() const {
    double total = 0.0;
    for (double probability : probabilities) total += probability;
    return total;
}

std::vector<double> TrainingSample::features() const {
    std::vector<double> out = public_state.features();
    out.insert(out.end(), belief.probabilities.begin(), belief.probabilities.end());
    return out;
}

double HeuristicValueEvaluator::evaluate(const PublicState& public_state, const BeliefState&, int player) const {
    return heuristic_from_public(public_state, player);
}

DepthLimitedResolver::DepthLimitedResolver(int iterations, int depth, const ValueEvaluator& evaluator, uint32_t seed)
    : iterations_(iterations),
      depth_(depth),
      evaluator_(evaluator),
      rng_(seed) {
    if (iterations_ <= 0) throw std::invalid_argument("resolver iterations must be positive");
    if (depth_ < 0) throw std::invalid_argument("resolver depth must be non-negative");
}

SearchResult DepthLimitedResolver::resolve(const GameState& root, int player) {
    if (player < 0 || player >= kPlayers) throw std::invalid_argument("invalid resolve player");
    nodes_.clear();
    for (int i = 0; i < iterations_; ++i) {
        GameState copy = root;
        traversal(copy, i % kPlayers, depth_, 1.0, 1.0);
    }

    SearchResult result;
    result.value = evaluator_.evaluate(public_state_from(root), belief_from_public_state(public_state_from(root)), player);
    if (!root.is_terminal()) {
        const InfosetKey key = root.infoset(player);
        const auto found = nodes_.find(key);
        if (found != nodes_.end()) result.policy = average_strategy(found->second.legal_mask, found->second.strategy_sum);
    }
    if (root.is_terminal() || result.policy == std::array<double, kActionCount>{}) {
        result.policy = regret_matching(root.legal_actions(), {});
    }
    return result;
}

double DepthLimitedResolver::traversal(GameState& state, int traverser, int remaining_depth,
                                       double reach0, double reach1) {
    if (state.is_terminal()) return state.utility(traverser);
    if (remaining_depth <= 0) {
        const PublicState public_state = public_state_from(state);
        const BeliefState belief = belief_from_public_state(public_state);
        return evaluator_.evaluate(public_state, belief, traverser);
    }

    const int player = state.current_player();
    LocalNode& node = node_for(state, player);
    const auto strategy = regret_matching(node.legal_mask, node.regret_sum);
    const double player_reach = player == 0 ? reach0 : reach1;
    for (Action action : actions_from_mask(node.legal_mask)) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        node.strategy_sum[index] += player_reach * strategy[index];
    }

    const std::vector<Action> actions = actions_from_mask(state.legal_actions());
    if (player != traverser) {
        std::vector<double> weights;
        weights.reserve(actions.size());
        for (Action action : actions) {
            weights.push_back(strategy[static_cast<std::size_t>(action_index(action))]);
        }
        std::discrete_distribution<int> dist(weights.begin(), weights.end());
        const Action action = actions[static_cast<std::size_t>(dist(rng_))];
        const double probability = strategy[static_cast<std::size_t>(action_index(action))];
        state.apply(action);
        const double value = player == 0
            ? traversal(state, traverser, remaining_depth - 1, reach0 * probability, reach1)
            : traversal(state, traverser, remaining_depth - 1, reach0, reach1 * probability);
        state.undo();
        return value;
    }

    std::array<double, kActionCount> action_values{};
    double node_value = 0.0;
    for (Action action : actions) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        state.apply(action);
        action_values[index] = player == 0
            ? traversal(state, traverser, remaining_depth - 1, reach0 * strategy[index], reach1)
            : traversal(state, traverser, remaining_depth - 1, reach0, reach1 * strategy[index]);
        state.undo();
        node_value += strategy[index] * action_values[index];
    }
    for (Action action : actions) {
        const std::size_t index = static_cast<std::size_t>(action_index(action));
        node.regret_sum[index] += action_values[index] - node_value;
    }
    return node_value;
}

DepthLimitedResolver::LocalNode& DepthLimitedResolver::node_for(const GameState& state, int player) {
    const InfosetKey key = state.infoset(player);
    auto [it, inserted] = nodes_.try_emplace(key);
    if (inserted) {
        it->second.key = key;
        it->second.legal_mask = state.legal_actions();
        it->second.regret_sum.fill(0.0);
        it->second.strategy_sum.fill(0.0);
    }
    return it->second;
}

std::size_t DepthLimitedResolver::LocalKeyHash::operator()(InfosetKey key) const {
    return static_cast<std::size_t>(key.value ^ (key.value >> 32));
}

std::array<Deal, kDealCount> all_deals() {
    const std::array<Card, 3> cards{Card::Assassin, Card::Contessa, Card::Civilian};
    std::array<Deal, kDealCount> deals{};
    std::size_t index = 0;
    for (Card p0 : cards) {
        for (Card p1 : cards) {
            if (p1 == p0) continue;
            for (Card hidden : cards) {
                if (hidden == p0 || hidden == p1) continue;
                deals[index++] = Deal{{p0, p1}, hidden};
            }
        }
    }
    return deals;
}

PublicState public_state_from(const GameState& state) {
    PublicState out;
    out.phase = state.phase();
    out.current_player = state.current_player();
    for (int player = 0; player < kPlayers; ++player) {
        out.coins[player] = state.coins(player);
        out.lives[player] = state.lives(player);
        out.assassinated[player] = state.has_assassinated(player);
    }
    out.legal_mask = state.legal_actions();
    out.history.reserve(static_cast<std::size_t>(state.public_history_size()));
    for (int i = 0; i < state.public_history_size(); ++i) out.history.push_back(state.public_event(i));
    return out;
}

BeliefState belief_from_public_state(const PublicState& public_state, int known_player, Card known_card) {
    BeliefState belief;
    belief.deals = all_deals();
    double total = 0.0;
    for (std::size_t i = 0; i < belief.deals.size(); ++i) {
        const Deal& deal = belief.deals[i];
        if (known_player >= 0 && deal.cards[static_cast<std::size_t>(known_player)] != known_card) continue;
        GameState replayed;
        if (!replay_public_history(deal, public_state.history, replayed)) continue;
        const PublicState replayed_public = public_state_from(replayed);
        if (!same_public_state(replayed_public, public_state)) continue;
        belief.probabilities[i] = 1.0;
        total += 1.0;
    }
    if (total > 0.0) {
        for (double& probability : belief.probabilities) probability /= total;
    }
    return belief;
}

bool replay_public_history(const Deal& deal, const std::vector<PublicEvent>& history, GameState& out) {
    try {
        GameState state(deal);
        for (const PublicEvent& event : history) {
            if (state.is_terminal()) return false;
            if (state.current_player() != event.player || !state.is_legal(event.action)) return false;
            state.apply(event.action);
        }
        out = state;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

TrainingSample make_training_sample(const GameState& state, DepthLimitedResolver& resolver,
                                    const ValueEvaluator& evaluator) {
    TrainingSample sample;
    sample.public_state = public_state_from(state);
    sample.belief = belief_from_public_state(sample.public_state);
    sample.player = state.current_player();
    sample.search_result = resolver.resolve(state, sample.player);
    sample.target_value = evaluator.evaluate(sample.public_state, sample.belief, sample.player);
    return sample;
}

std::vector<TrainingSample> generate_training_samples(int samples, int max_steps, uint32_t seed,
                                                      int resolve_iterations, int resolve_depth) {
    if (samples < 0 || max_steps < 0) throw std::invalid_argument("sample counts must be non-negative");
    std::mt19937 rng(seed);
    const auto deals = all_deals();
    HeuristicValueEvaluator evaluator;
    DepthLimitedResolver resolver(resolve_iterations, resolve_depth, evaluator, seed + 17);
    std::vector<TrainingSample> rows;
    rows.reserve(static_cast<std::size_t>(samples));

    for (int sample = 0; sample < samples; ++sample) {
        GameState state(deals[static_cast<std::size_t>(rng() % deals.size())]);
        for (int step = 0; step < max_steps && !state.is_terminal(); ++step) {
            TrainingSample sample_row = make_training_sample(state, resolver, evaluator);
            rows.push_back(sample_row);
            if (static_cast<int>(rows.size()) >= samples) return rows;

            const std::vector<Action> actions = actions_from_mask(state.legal_actions());
            std::vector<double> weights;
            weights.reserve(actions.size());
            for (Action action : actions) {
                weights.push_back(std::max(0.0, sample_row.search_result.policy[static_cast<std::size_t>(action_index(action))]));
            }
            if (std::accumulate(weights.begin(), weights.end(), 0.0) <= 1e-12) {
                weights.assign(actions.size(), 1.0);
            }
            std::discrete_distribution<int> dist(weights.begin(), weights.end());
            state.apply(actions[static_cast<std::size_t>(dist(rng))]);
        }
    }
    return rows;
}

} // namespace small_coup::rebel
