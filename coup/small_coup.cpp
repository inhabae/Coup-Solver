#include "small_coup.hpp"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace small_coup {

namespace {

constexpr double kRegretEpsilon = 1e-15;

int action_index(Action action) {
    return static_cast<int>(action);
}

int card_index(Card card) {
    return static_cast<int>(card);
}

uint64_t mix(uint64_t seed, uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

} // namespace

ActionMask action_bit(Action action) {
    return ActionMask{1} << static_cast<unsigned>(action);
}

std::vector<Action> actions_from_mask(ActionMask mask) {
    std::vector<Action> actions;
    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        const Action action = static_cast<Action>(i);
        if ((mask & action_bit(action)) != 0) actions.push_back(action);
    }
    return actions;
}

const char* card_name(Card card) {
    switch (card) {
        case Card::Assassin: return "Assassin";
        case Card::Contessa: return "Contessa";
        case Card::Civilian: return "Civilian";
        case Card::None: return "None";
    }
    return "Unknown";
}

const char* action_name(Action action) {
    switch (action) {
        case Action::Income: return "Income";
        case Action::Assassinate: return "Assassinate";
        case Action::Coup: return "Coup";
        case Action::Allow: return "Allow";
        case Action::BlockAssassinate: return "BlockAssassinate";
        case Action::Challenge: return "Challenge";
        case Action::LoseLife: return "LoseLife";
        case Action::Count: return "Count";
    }
    return "Unknown";
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::TurnAction: return "TurnAction";
        case Phase::RespondToAssassinate: return "RespondToAssassinate";
        case Phase::RespondToBlock: return "RespondToBlock";
        case Phase::LoseLife: return "LoseLife";
        case Phase::Terminal: return "Terminal";
    }
    return "Unknown";
}

GameState::GameState() {
    reset(Deal{{Card::Assassin, Card::Contessa}, Card::Civilian});
}

GameState::GameState(const Deal& deal) {
    reset(deal);
}

void GameState::reset(const Deal& deal) {
    std::array<int, 3> counts{0, 0, 0};
    for (Card card : deal.cards) {
        if (card == Card::None) throw std::invalid_argument("player card cannot be None");
        ++counts[static_cast<std::size_t>(card_index(card))];
    }
    if (deal.hidden == Card::None) throw std::invalid_argument("hidden card cannot be None");
    ++counts[static_cast<std::size_t>(card_index(deal.hidden))];
    for (int count : counts) {
        if (count != 1) throw std::invalid_argument("small Coup deal must contain each card exactly once");
    }

    undo_stack_.clear();
    phase_ = Phase::TurnAction;
    current_player_ = 0;
    coins_ = {kStartingCoins, kStartingCoins};
    lives_ = {kStartingLives, kStartingLives};
    assassinated_ = {false, false};
    cards_ = deal.cards;
    hidden_ = deal.hidden;
    pending_ = Pending{};
    public_history_len_ = 0;
}

bool GameState::is_terminal() const {
    return phase_ == Phase::Terminal;
}

int GameState::current_player() const {
    return current_player_;
}

Phase GameState::phase() const {
    return phase_;
}

ActionMask GameState::legal_actions() const {
    if (phase_ == Phase::Terminal) return 0;
    if (phase_ == Phase::TurnAction) {
        if (coins_[current_player_] >= kCoupCost) return action_bit(Action::Coup);
        ActionMask mask = action_bit(Action::Income);
        if (coins_[current_player_] >= kAssassinateCost && !assassinated_[current_player_]) {
            mask |= action_bit(Action::Assassinate);
        }
        return mask;
    }
    if (phase_ == Phase::RespondToAssassinate) {
        if (cards_[current_player_] == Card::Contessa) {
            return action_bit(Action::BlockAssassinate);
        }
        return action_bit(Action::Allow) | action_bit(Action::Challenge) | action_bit(Action::BlockAssassinate);
    }
    if (phase_ == Phase::RespondToBlock) return action_bit(Action::Allow) | action_bit(Action::Challenge);
    if (phase_ == Phase::LoseLife) return action_bit(Action::LoseLife);
    return 0;
}

bool GameState::is_legal(Action action) const {
    return (legal_actions() & action_bit(action)) != 0;
}

void GameState::apply(Action action) {
    if (!is_legal(action)) throw std::invalid_argument("illegal small Coup action");
    undo_stack_.push_back(snapshot());
    append_event(action, current_player_);

    if (phase_ == Phase::TurnAction) {
        pending_ = Pending{action, current_player_, opponent(current_player_), -1};
        if (action == Action::Income) {
            ++coins_[current_player_];
            end_turn(pending_.target);
        } else if (action == Action::Assassinate) {
            coins_[current_player_] -= kAssassinateCost;
            assassinated_[current_player_] = true;
            current_player_ = pending_.target;
            phase_ = Phase::RespondToAssassinate;
        } else if (action == Action::Coup) {
            coins_[current_player_] -= kCoupCost;
            current_player_ = pending_.target;
            phase_ = Phase::LoseLife;
        }
        return;
    }

    if (phase_ == Phase::RespondToAssassinate) {
        if (action == Action::Allow) {
            phase_ = Phase::LoseLife;
        } else if (action == Action::Challenge) {
            current_player_ = cards_[pending_.actor] == Card::Assassin ? pending_.target : pending_.actor;
            phase_ = Phase::LoseLife;
        } else if (action == Action::BlockAssassinate) {
            pending_.block_actor = current_player_;
            current_player_ = pending_.actor;
            phase_ = Phase::RespondToBlock;
        }
        return;
    }

    if (phase_ == Phase::RespondToBlock) {
        if (action == Action::Allow) {
            end_turn(opponent(pending_.actor));
        } else {
            current_player_ = cards_[pending_.block_actor] == Card::Contessa
                ? pending_.actor
                : pending_.block_actor;
            phase_ = Phase::LoseLife;
        }
        return;
    }

    if (phase_ == Phase::LoseLife) {
        --lives_[current_player_];
        if (lives_[current_player_] <= 0) {
            phase_ = Phase::Terminal;
        } else {
            end_turn(opponent(pending_.actor));
        }
    }
}

void GameState::undo() {
    if (undo_stack_.empty()) throw std::runtime_error("undo stack is empty");
    restore(undo_stack_.back());
    undo_stack_.pop_back();
}

double GameState::utility(int player) const {
    if (!is_terminal()) throw std::runtime_error("utility called on non-terminal small Coup state");
    const int other = opponent(player);
    if (lives_[player] > 0 && lives_[other] == 0) return 1.0;
    if (lives_[player] == 0 && lives_[other] > 0) return -1.0;
    return 0.0;
}

InfosetKey GameState::infoset(int player) const {
    uint64_t key = 1469598103934665603ULL;
    key = mix(key, static_cast<uint64_t>(phase_));
    key = mix(key, static_cast<uint64_t>(current_player_));
    key = mix(key, static_cast<uint64_t>(player));
    key = mix(key, static_cast<uint64_t>(card_index(cards_[player])));
    key = mix(key, static_cast<uint64_t>(coins_[0]));
    key = mix(key, static_cast<uint64_t>(coins_[1]));
    key = mix(key, static_cast<uint64_t>(lives_[0]));
    key = mix(key, static_cast<uint64_t>(lives_[1]));
    key = mix(key, assassinated_[0] ? 1ULL : 0ULL);
    key = mix(key, assassinated_[1] ? 1ULL : 0ULL);
    for (int i = 0; i < public_history_len_; ++i) {
        key = mix(key, static_cast<uint64_t>(public_history_[i].player + 1));
        key = mix(key, static_cast<uint64_t>(public_history_[i].action));
    }
    return InfosetKey{key};
}

InfosetMetadata GameState::infoset_metadata(int player) const {
    InfosetMetadata metadata;
    metadata.player = player;
    metadata.private_card = cards_[player];
    metadata.phase = phase_;
    metadata.current_player = current_player_;
    metadata.coins = coins_;
    metadata.lives = lives_;
    metadata.assassinated = assassinated_;
    metadata.history.reserve(static_cast<std::size_t>(public_history_len_));
    for (int i = 0; i < public_history_len_; ++i) metadata.history.push_back(public_history_[i]);
    return metadata;
}

std::string GameState::infoset_string(int player) const {
    std::ostringstream out;
    out << "P" << player
        << " card=" << card_name(cards_[player])
        << " phase=" << phase_name(phase_)
        << " current=P" << current_player_
        << " coins=(" << coins_[0] << "," << coins_[1] << ")"
        << " lives=(" << lives_[0] << "," << lives_[1] << ")"
        << " assassinated=(" << (assassinated_[0] ? "Y" : "N")
        << "," << (assassinated_[1] ? "Y" : "N") << ")"
        << " history=[";
    for (int i = 0; i < public_history_len_; ++i) {
        if (i > 0) out << ",";
        out << "P" << public_history_[i].player << ":" << action_name(public_history_[i].action);
    }
    out << "]";
    return out.str();
}

int GameState::coins(int player) const {
    return coins_[player];
}

int GameState::lives(int player) const {
    return lives_[player];
}

bool GameState::has_assassinated(int player) const {
    return assassinated_[player];
}

Card GameState::card(int player) const {
    return cards_[player];
}

Card GameState::hidden_card() const {
    return hidden_;
}

int GameState::public_history_size() const {
    return public_history_len_;
}

const PublicEvent& GameState::public_event(int index) const {
    return public_history_[index];
}

std::string GameState::debug_string() const {
    std::ostringstream out;
    out << "phase=" << phase_name(phase_) << " current=" << current_player_ << "\n";
    for (int p = 0; p < kPlayers; ++p) {
        out << "P" << p << " coins=" << coins_[p]
            << " lives=" << lives_[p]
            << " assassinated=" << (assassinated_[p] ? "Y" : "N")
            << " card=" << card_name(cards_[p]) << "\n";
    }
    out << "hidden=" << card_name(hidden_) << "\nhistory=";
    for (int i = 0; i < public_history_len_; ++i) {
        out << " P" << public_history_[i].player << ":" << action_name(public_history_[i].action);
    }
    return out.str();
}

GameState::Snapshot GameState::snapshot() const {
    return Snapshot{phase_, current_player_, coins_, lives_, assassinated_, cards_, hidden_, pending_,
                    public_history_len_};
}

void GameState::restore(const Snapshot& snapshot) {
    phase_ = snapshot.phase;
    current_player_ = snapshot.current_player;
    coins_ = snapshot.coins;
    lives_ = snapshot.lives;
    assassinated_ = snapshot.assassinated;
    cards_ = snapshot.cards;
    hidden_ = snapshot.hidden;
    pending_ = snapshot.pending;
    public_history_len_ = snapshot.public_history_len;
}

void GameState::append_event(Action action, int player) {
    assert(public_history_len_ < kMaxPublicHistory);
    public_history_[public_history_len_++] = PublicEvent{action, player};
}

void GameState::end_turn(int next_player) {
    if (lives_[0] <= 0 || lives_[1] <= 0) {
        phase_ = Phase::Terminal;
        return;
    }
    current_player_ = next_player;
    phase_ = Phase::TurnAction;
    pending_ = Pending{};
}

int GameState::opponent(int player) const {
    return 1 - player;
}

InfosetNode::InfosetNode(InfosetKey infoset_key, InfosetMetadata infoset_metadata, std::string infoset_label,
                         ActionMask actions)
    : key(infoset_key),
      metadata(std::move(infoset_metadata)),
      label(std::move(infoset_label)),
      legal_mask(actions),
      regret_sum(static_cast<std::size_t>(Action::Count), 0.0),
      strategy_sum(static_cast<std::size_t>(Action::Count), 0.0),
      current_strategy(static_cast<std::size_t>(Action::Count), 0.0) {}

std::vector<double> InfosetNode::strategy() {
    const std::vector<Action> actions = actions_from_mask(legal_mask);
    if (actions.empty()) throw std::runtime_error("small Coup infoset has no legal actions");

    std::fill(current_strategy.begin(), current_strategy.end(), 0.0);
    double normalizer = 0.0;
    for (Action action : actions) {
        const int index = action_index(action);
        current_strategy[static_cast<std::size_t>(index)] =
            std::max(0.0, regret_sum[static_cast<std::size_t>(index)]);
        normalizer += current_strategy[static_cast<std::size_t>(index)];
    }

    const double fallback = 1.0 / static_cast<double>(actions.size());
    for (Action action : actions) {
        const int index = action_index(action);
        current_strategy[static_cast<std::size_t>(index)] =
            normalizer > kRegretEpsilon
                ? current_strategy[static_cast<std::size_t>(index)] / normalizer
                : fallback;
    }
    return current_strategy;
}

void InfosetNode::accumulate_strategy(const std::vector<double>& strategy, double realization_weight) {
    for (Action action : actions_from_mask(legal_mask)) {
        const int index = action_index(action);
        strategy_sum[static_cast<std::size_t>(index)] +=
            realization_weight * strategy[static_cast<std::size_t>(index)];
    }
}

std::vector<double> InfosetNode::average_strategy() const {
    std::vector<double> average(static_cast<std::size_t>(Action::Count), 0.0);
    const std::vector<Action> actions = actions_from_mask(legal_mask);
    double normalizer = 0.0;
    for (Action action : actions) normalizer += strategy_sum[static_cast<std::size_t>(action_index(action))];

    const double fallback = actions.empty() ? 0.0 : 1.0 / static_cast<double>(actions.size());
    for (Action action : actions) {
        const int index = action_index(action);
        average[static_cast<std::size_t>(index)] =
            normalizer > kRegretEpsilon
                ? strategy_sum[static_cast<std::size_t>(index)] / normalizer
                : fallback;
    }
    return average;
}

std::size_t InfosetKeyHash::operator()(InfosetKey key) const {
    return static_cast<std::size_t>(key.value ^ (key.value >> 32));
}

CfrTrainer::CfrTrainer(uint32_t seed, int max_public_actions)
    : rng_(seed), max_public_actions_(max_public_actions) {
    if (max_public_actions_ <= 0 || max_public_actions_ >= kMaxPublicHistory) {
        throw std::invalid_argument("max public actions must be between 1 and kMaxPublicHistory - 1");
    }
}

TrainingStats CfrTrainer::train(int iterations) {
    if (iterations < 0) throw std::invalid_argument("iterations must be non-negative");
    TrainingStats stats;
    for (int i = 0; i < iterations; ++i) {
        const int traverser = i % kPlayers;
        const Deal deal = sample_deal();
        const double utility = run_traversal(traverser, deal);
        stats.utility0_sum += traverser == 0 ? utility : -utility;
        ++stats.iterations;
    }
    stats.infosets = nodes_.size();
    return stats;
}

const std::unordered_map<InfosetKey, InfosetNode, InfosetKeyHash>& CfrTrainer::nodes() const {
    return nodes_;
}

double CfrTrainer::run_traversal(int traverser, const Deal& deal) {
    GameState state(deal);
    return cfr(state, traverser, 1.0, 1.0);
}

double CfrTrainer::cfr(GameState& state, int traverser, double reach0, double reach1) {
    if (state.is_terminal()) return state.utility(traverser);
    if (state.public_history_size() >= max_public_actions_) {
        return depth_limited_utility(state, traverser);
    }

    const int player = state.current_player();
    InfosetNode& node = node_for(state, player);
    const std::vector<double> strategy = node.strategy();
    node.accumulate_strategy(strategy, player == 0 ? reach0 : reach1);
    const std::vector<Action> actions = actions_from_mask(state.legal_actions());

    double node_value = 0.0;
    std::vector<double> action_values(static_cast<std::size_t>(Action::Count), 0.0);
    for (Action action : actions) {
        const int index = action_index(action);
        const double probability = strategy[static_cast<std::size_t>(index)];
        state.apply(action);
        action_values[static_cast<std::size_t>(index)] =
            player == 0
                ? cfr(state, traverser, reach0 * probability, reach1)
                : cfr(state, traverser, reach0, reach1 * probability);
        state.undo();
        node_value += probability * action_values[static_cast<std::size_t>(index)];
    }

    if (player == traverser) {
        const double opponent_reach = player == 0 ? reach1 : reach0;
        for (Action action : actions) {
            const int index = action_index(action);
            const double regret = action_values[static_cast<std::size_t>(index)] - node_value;
            node.regret_sum[static_cast<std::size_t>(index)] += opponent_reach * regret;
        }
    }

    return node_value;
}

double CfrTrainer::depth_limited_utility(const GameState& state, int traverser) const {
    const int opponent = 1 - traverser;
    double value = static_cast<double>(state.lives(traverser) - state.lives(opponent));
    value += 0.1 * static_cast<double>(state.coins(traverser) - state.coins(opponent));
    return std::max(-1.0, std::min(1.0, value));
}

InfosetNode& CfrTrainer::node_for(const GameState& state, int player) {
    const InfosetKey key = state.infoset(player);
    const InfosetMetadata metadata = state.infoset_metadata(player);
    const std::string label = state.infoset_string(player);
    const ActionMask legal_mask = state.legal_actions();
    auto [it, inserted] = nodes_.try_emplace(key, key, metadata, label, legal_mask);
    if (!inserted && it->second.legal_mask != legal_mask) {
        throw std::runtime_error("small Coup infoset key collision");
    }
    if (!inserted && it->second.label != label) {
        throw std::runtime_error("small Coup infoset label collision");
    }
    return it->second;
}

Deal CfrTrainer::sample_deal() {
    std::array<Card, 3> deck{Card::Assassin, Card::Contessa, Card::Civilian};
    std::shuffle(deck.begin(), deck.end(), rng_);
    return Deal{{deck[0], deck[1]}, deck[2]};
}

} // namespace small_coup
