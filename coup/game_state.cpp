#include "game_state.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace coup {

namespace {

int card_index(Card card) {
    assert(card != Card::None);
    return static_cast<int>(card);
}

bool is_keep_pair(Action action) {
    return action >= Action::Keep01 && action <= Action::Keep23;
}

bool is_keep_single(Action action) {
    return action >= Action::Keep0 && action <= Action::Keep3;
}

const char* observation_kind_name(ObservationTokenKind kind) {
    switch (kind) {
        case ObservationTokenKind::InitialCards: return "InitialCards";
        case ObservationTokenKind::PublicAction: return "PublicAction";
        case ObservationTokenKind::PublicReveal: return "PublicReveal";
        case ObservationTokenKind::PublicLose: return "PublicLose";
        case ObservationTokenKind::PublicReplacement: return "PublicReplacement";
        case ObservationTokenKind::PublicExchangeComplete: return "PublicExchangeComplete";
        case ObservationTokenKind::PrivateReplacement: return "PrivateReplacement";
        case ObservationTokenKind::PrivateExchangeDraw: return "PrivateExchangeDraw";
        case ObservationTokenKind::PrivateExchangeKeep: return "PrivateExchangeKeep";
    }
    return "Unknown";
}

Card token_card(uint8_t card) {
    return static_cast<Card>(card);
}

Action token_action(uint8_t action) {
    return static_cast<Action>(action);
}

std::array<int, 2> keep_pair_positions(Action action) {
    switch (action) {
        case Action::Keep01: return {0, 1};
        case Action::Keep02: return {0, 2};
        case Action::Keep03: return {0, 3};
        case Action::Keep12: return {1, 2};
        case Action::Keep13: return {1, 3};
        case Action::Keep23: return {2, 3};
        default: break;
    }
    assert(false && "not a keep-pair action");
    return {0, 1};
}

int keep_single_position(Action action) {
    switch (action) {
        case Action::Keep0: return 0;
        case Action::Keep1: return 1;
        case Action::Keep2: return 2;
        case Action::Keep3: return 3;
        default: break;
    }
    assert(false && "not a keep-single action");
    return 0;
}

} // namespace

ActionMask action_bit(Action action) {
    return ActionMask{1} << static_cast<unsigned>(action);
}

const char* card_name(Card card) {
    switch (card) {
        case Card::Duke: return "Duke";
        case Card::Assassin: return "Assassin";
        case Card::Captain: return "Captain";
        case Card::Ambassador: return "Ambassador";
        case Card::Contessa: return "Contessa";
        case Card::None: return "None";
    }
    return "Unknown";
}

const char* action_name(Action action) {
    switch (action) {
        case Action::Income: return "Income";
        case Action::ForeignAid: return "ForeignAid";
        case Action::Tax: return "Tax";
        case Action::Steal: return "Steal";
        case Action::Exchange: return "Exchange";
        case Action::Assassinate: return "Assassinate";
        case Action::Coup: return "Coup";
        case Action::Allow: return "Allow";
        case Action::Challenge: return "Challenge";
        case Action::BlockForeignAidDuke: return "BlockForeignAidDuke";
        case Action::BlockStealCaptain: return "BlockStealCaptain";
        case Action::BlockStealAmbassador: return "BlockStealAmbassador";
        case Action::BlockAssassinateContessa: return "BlockAssassinateContessa";
        case Action::RevealSlot0: return "RevealSlot0";
        case Action::RevealSlot1: return "RevealSlot1";
        case Action::LoseSlot0: return "LoseSlot0";
        case Action::LoseSlot1: return "LoseSlot1";
        case Action::Keep0: return "Keep0";
        case Action::Keep1: return "Keep1";
        case Action::Keep2: return "Keep2";
        case Action::Keep3: return "Keep3";
        case Action::Keep01: return "Keep01";
        case Action::Keep02: return "Keep02";
        case Action::Keep03: return "Keep03";
        case Action::Keep12: return "Keep12";
        case Action::Keep13: return "Keep13";
        case Action::Keep23: return "Keep23";
        case Action::ClaimMate: return "ClaimMate";
        case Action::Count: return "Count";
    }
    return "Unknown";
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::TurnAction: return "TurnAction";
        case Phase::ResponseToAction: return "ResponseToAction";
        case Phase::ResponseToBlock: return "ResponseToBlock";
        case Phase::ChallengeReveal: return "ChallengeReveal";
        case Phase::LoseInfluence: return "LoseInfluence";
        case Phase::Redraw: return "Redraw";
        case Phase::ExchangeDraw: return "ExchangeDraw";
        case Phase::ExchangeChoose: return "ExchangeChoose";
        case Phase::Terminal: return "Terminal";
    }
    return "Unknown";
}

GameState::GameState() {
    observation_store_ = make_observation_store();
    reset();
}

GameState::GameState(const Deal& deal) {
    observation_store_ = make_observation_store();
    reset(deal);
}

GameState::GameState(ObservationStorePtr observation_store)
    : observation_store_(std::move(observation_store)) {
    if (!observation_store_) observation_store_ = make_observation_store();
    reset();
}

GameState::GameState(const Deal& deal, ObservationStorePtr observation_store)
    : observation_store_(std::move(observation_store)) {
    if (!observation_store_) observation_store_ = make_observation_store();
    reset(deal);
}

void GameState::reset() {
    Deal deal;
    deal.cards = {{{{Card::Duke, Card::Assassin}}, {{Card::Captain, Card::Ambassador}}}};
    reset(deal);
}

void GameState::reset(const Deal& deal) {
    undo_stack_.clear();
    current_player_ = 0;
    phase_ = Phase::TurnAction;
    coins_ = {kStartingCoins, kStartingCoins};
    cards_ = deal.cards;
    live_ = {{{true, true}, {true, true}}};
    steal_allowed_restricted_ = {false, false};
    foreign_aid_block_allowed_restricted_ = {false, false};
    steal_block_allowed_restricted_ = {false, false};
    assassinate_block_allowed_restricted_ = {false, false};
    duke_claimed_ = {false, false};
    last_turn_action_ = {Action::Count, Action::Count};
    pending_ = Pending{};
    exchange_cards_.fill(Card::None);
    public_history_len_ = 0;
    post_2v2_start_history_len_ = -1;
    initialize_deck();
    remove_dealt_cards();
    initialize_observations();
}

void GameState::set_deal(const Deal& deal) {
    reset(deal);
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
    if (is_chance_node() || phase_ == Phase::Terminal) return 0;
    switch (phase_) {
        case Phase::TurnAction: return turn_action_mask();
        case Phase::ResponseToAction: return response_to_action_mask();
        case Phase::ResponseToBlock: return response_to_block_mask();
        case Phase::ChallengeReveal: return challenge_reveal_mask();
        case Phase::LoseInfluence: return lose_influence_mask();
        case Phase::ExchangeChoose: return exchange_choose_mask();
        default: return 0;
    }
}

bool GameState::is_legal(Action action) const {
    return (legal_actions() & action_bit(action)) != 0;
}

void GameState::apply(Action action) {
    if (!is_legal(action)) throw std::invalid_argument("illegal action");
    save_undo();
    switch (phase_) {
        case Phase::TurnAction:
            apply_turn_action(action);
            break;
        case Phase::ResponseToAction:
            apply_action_response(action);
            break;
        case Phase::ResponseToBlock:
            apply_block_response(action);
            break;
        case Phase::ChallengeReveal:
            apply_challenge_reveal(action);
            break;
        case Phase::LoseInfluence:
            apply_lose_influence(action);
            break;
        case Phase::ExchangeChoose:
            apply_exchange_choose(action);
            break;
        default:
            assert(false && "apply called in non-action phase");
            break;
    }
}

void GameState::undo() {
    if (undo_stack_.empty()) throw std::runtime_error("undo stack is empty");
    restore(undo_stack_.back());
    undo_stack_.pop_back();
}

bool GameState::is_chance_node() const {
    return phase_ == Phase::Redraw || phase_ == Phase::ExchangeDraw;
}

std::vector<ChanceOutcome> GameState::chance_outcomes() const {
    std::vector<ChanceOutcome> outcomes;
    if (phase_ == Phase::Redraw) {
        int total = 0;
        for (int count : deck_) total += count;
        for (int c = 0; c < kCardTypes; ++c) {
            if (deck_[c] == 0) continue;
            outcomes.push_back({ChanceType::Redraw, static_cast<Card>(c), Card::None,
                                static_cast<double>(deck_[c]) / total});
        }
    } else if (phase_ == Phase::ExchangeDraw) {
        int total = 0;
        for (int count : deck_) total += count;
        const double denom = static_cast<double>(total * (total - 1) / 2);
        for (int a = 0; a < kCardTypes; ++a) {
            for (int b = a; b < kCardTypes; ++b) {
                double ways = 0.0;
                if (a == b) {
                    ways = deck_[a] >= 2 ? deck_[a] * (deck_[a] - 1) / 2.0 : 0.0;
                } else {
                    ways = static_cast<double>(deck_[a] * deck_[b]);
                }
                if (ways == 0.0) continue;
                outcomes.push_back({ChanceType::ExchangeDraw, static_cast<Card>(a), static_cast<Card>(b),
                                    ways / denom});
            }
        }
    }
    return outcomes;
}

void GameState::apply_chance(const ChanceOutcome& outcome) {
    if (!is_chance_node()) throw std::invalid_argument("not a chance node");
    save_undo();
    if (phase_ == Phase::Redraw) {
        if (outcome.type != ChanceType::Redraw) throw std::invalid_argument("wrong chance outcome type");
        assert(pending_.revealed_slot >= 0);
        --deck_[card_index(outcome.first)];
        cards_[pending_.challenged_player][pending_.revealed_slot] = outcome.first;
        append_public_observation(public_replacement_token(pending_.challenged_player));
        append_private_observation(pending_.challenged_player,
                                   private_replacement_token(pending_.challenged_player, outcome.first));
        finish_truthful_challenge_after_redraw();
    } else {
        if (outcome.type != ChanceType::ExchangeDraw) throw std::invalid_argument("wrong chance outcome type");
        --deck_[card_index(outcome.first)];
        --deck_[card_index(outcome.second)];
        exchange_cards_.fill(Card::None);
        const int actor = pending_.actor;
        exchange_cards_[0] = live_[actor][0] ? cards_[actor][0] : Card::None;
        exchange_cards_[1] = live_[actor][1] ? cards_[actor][1] : Card::None;
        exchange_cards_[2] = outcome.first;
        exchange_cards_[3] = outcome.second;
        append_private_observation(actor, private_exchange_draw_token(actor, outcome.first, outcome.second));
        current_player_ = actor;
        phase_ = Phase::ExchangeChoose;
    }
}

void GameState::undo_chance() {
    undo();
}

double GameState::utility(int player) const {
    if (!is_terminal()) throw std::runtime_error("utility called on non-terminal state");
    const int other = opponent(player);
    if (live_count(player) > 0 && live_count(other) == 0) return 1.0;
    if (live_count(player) == 0 && live_count(other) > 0) return -1.0;
    return 0.0;
}

bool InfosetKey::operator==(const InfosetKey& other) const {
    return public_obs_id == other.public_obs_id &&
           private_obs_id == other.private_obs_id;
}

InfosetKey GameState::infoset(int player) const {
    return InfosetKey{public_obs_id_, private_obs_id_[player]};
}

int GameState::coins(int player) const {
    return coins_[player];
}

bool GameState::live(int player, int slot) const {
    return live_[player][slot];
}

Card GameState::card(int player, int slot) const {
    return cards_[player][slot];
}

int GameState::deck_count(Card card) const {
    return deck_[card_index(card)];
}

int GameState::public_history_size() const {
    return public_history_len_;
}

bool GameState::is_2v2() const {
    return live_[0][0] && live_[0][1] && live_[1][0] && live_[1][1];
}

int GameState::post_2v2_public_history_size() const {
    if (post_2v2_start_history_len_ < 0) return 0;
    return public_history_len_ - post_2v2_start_history_len_;
}

const PublicEvent& GameState::public_event(int index) const {
    return public_history_[index];
}

uint32_t GameState::public_observation_id() const {
    return public_obs_id_;
}

uint32_t GameState::private_observation_id(int player) const {
    return private_obs_id_[player];
}

std::string GameState::debug_string() const {
    std::ostringstream out;
    out << "phase=" << phase_name(phase_) << " current=" << current_player_ << "\n";
    for (int p = 0; p < kPlayers; ++p) {
        out << "P" << p << " coins=" << coins_[p] << " cards=";
        for (int s = 0; s < kInfluence; ++s) {
            out << card_name(cards_[p][s]) << (live_[p][s] ? "(live)" : "(dead)") << " ";
        }
        out << "\n";
    }
    out << "deck=";
    for (int c = 0; c < kCardTypes; ++c) out << card_name(static_cast<Card>(c)) << ":" << deck_[c] << " ";
    out << "\nhistory=";
    for (int i = 0; i < public_history_len_; ++i) {
        out << action_name(public_history_[i].action) << " ";
    }
    return out.str();
}

std::string GameState::infoset_debug_string(int player) const {
    const InfosetKey key = infoset(player);
    std::ostringstream out;
    out << "key public=" << key.public_obs_id
        << " private=" << key.private_obs_id
        << " player=" << player
        << " phase=" << phase_name(phase_)
        << " current=" << current_player_;

    auto append_chain = [&](const char* label, uint32_t id) {
        std::vector<ObservationToken> chain;
        while (id != 0) {
            const ObservationNode& node = observation_store_->node(id);
            chain.push_back(node.token);
            id = node.parent;
        }
        std::reverse(chain.begin(), chain.end());
        out << "\n" << label << "=";
        for (const ObservationToken& token : chain) {
            out << " [" << observation_kind_name(token.kind)
                << " p=" << static_cast<int>(token.player);
            if (token.kind == ObservationTokenKind::PublicAction ||
                token.kind == ObservationTokenKind::PublicReveal ||
                token.kind == ObservationTokenKind::PublicLose ||
                token.kind == ObservationTokenKind::PrivateExchangeKeep) {
                out << " a=" << action_name(token_action(token.action));
            }
            if (token.card0 != static_cast<uint8_t>(Card::None)) out << " c0=" << card_name(token_card(token.card0));
            if (token.card1 != static_cast<uint8_t>(Card::None)) out << " c1=" << card_name(token_card(token.card1));
            if (token.card2 != static_cast<uint8_t>(Card::None)) out << " c2=" << card_name(token_card(token.card2));
            if (token.card3 != static_cast<uint8_t>(Card::None)) out << " c3=" << card_name(token_card(token.card3));
            out << "]";
        }
    };

    append_chain("public", public_obs_id_);
    append_chain("private", private_obs_id_[player]);
    return out.str();
}

GameState::Snapshot GameState::snapshot() const {
    return Snapshot{phase_, current_player_, coins_, cards_, live_, steal_allowed_restricted_,
                    foreign_aid_block_allowed_restricted_, steal_block_allowed_restricted_,
                    assassinate_block_allowed_restricted_, duke_claimed_, last_turn_action_,
                    deck_, pending_, exchange_cards_,
                    public_history_len_, post_2v2_start_history_len_, public_obs_id_, private_obs_id_};
}

void GameState::restore(const Snapshot& snapshot) {
    phase_ = snapshot.phase;
    current_player_ = snapshot.current_player;
    coins_ = snapshot.coins;
    cards_ = snapshot.cards;
    live_ = snapshot.live;
    steal_allowed_restricted_ = snapshot.steal_allowed_restricted;
    foreign_aid_block_allowed_restricted_ = snapshot.foreign_aid_block_allowed_restricted;
    steal_block_allowed_restricted_ = snapshot.steal_block_allowed_restricted;
    assassinate_block_allowed_restricted_ = snapshot.assassinate_block_allowed_restricted;
    duke_claimed_ = snapshot.duke_claimed;
    last_turn_action_ = snapshot.last_turn_action;
    deck_ = snapshot.deck;
    pending_ = snapshot.pending;
    exchange_cards_ = snapshot.exchange_cards;
    public_history_len_ = snapshot.public_history_len;
    post_2v2_start_history_len_ = snapshot.post_2v2_start_history_len;
    public_obs_id_ = snapshot.public_obs_id;
    private_obs_id_ = snapshot.private_obs_id;
}

void GameState::save_undo() {
    undo_stack_.push_back(snapshot());
}

void GameState::initialize_deck() {
    deck_.fill(3);
}

void GameState::remove_dealt_cards() {
    for (int p = 0; p < kPlayers; ++p) {
        for (int s = 0; s < kInfluence; ++s) {
            const int index = card_index(cards_[p][s]);
            if (deck_[index] <= 0) {
                throw std::invalid_argument("deal contains too many copies of a card");
            }
            --deck_[index];
        }
    }
}

void GameState::initialize_observations() {
    if (!observation_store_) observation_store_ = make_observation_store();
    public_obs_id_ = 0;
    for (int p = 0; p < kPlayers; ++p) {
        private_obs_id_[p] = observation_store_->append(
            0, initial_cards_token(p, cards_[p][0], cards_[p][1]));
    }
}

void GameState::append_event(Action action, int player, Card card) {
    assert(public_history_len_ < kMaxPublicHistory);
    public_history_[public_history_len_++] = PublicEvent{action, player, card};
    append_public_action_observation(action, player, card);
}

void GameState::append_public_observation(const ObservationToken& token) {
    public_obs_id_ = observation_store_->append(public_obs_id_, token);
}

void GameState::append_private_observation(int player, const ObservationToken& token) {
    private_obs_id_[player] = observation_store_->append(private_obs_id_[player], token);
}

void GameState::append_public_action_observation(Action action, int player, Card card) {
    if (action >= Action::Keep0 && action <= Action::Keep23) return;
    if (action == Action::RevealSlot0 || action == Action::RevealSlot1) {
        append_public_observation(public_reveal_token(player, action, card));
    } else if (action == Action::LoseSlot0 || action == Action::LoseSlot1) {
        append_public_observation(public_lose_token(player, action, card));
    } else {
        append_public_observation(public_action_token(player, action));
    }
}

ActionMask GameState::turn_action_mask() const {
    return turn_action_mask_for(current_player_, coins_);
}

ActionMask GameState::turn_action_mask_for(int player, const std::array<int, kPlayers>& coins) const {
    if (coins[player] >= kForcedCoupCoins) return action_bit(Action::Coup);
    if (coins[player] >= kCoupCost && live_count(opponent(player)) == 1) return action_bit(Action::Coup);
    if (can_claim_mate(player, coins)) return action_bit(Action::ClaimMate);
    ActionMask mask = action_bit(Action::Income) | action_bit(Action::ForeignAid) |
                      action_bit(Action::Tax) | action_bit(Action::Steal);
                      // action_bit(Action::Exchange);
    if (coins[player] >= kAssassinateCost) mask |= action_bit(Action::Assassinate);
    if (coins[player] >= kCoupCost) mask |= action_bit(Action::Coup);
    return apply_last_turn_action_restriction(
        player,
        apply_duke_claim_restriction(
            player, apply_block_allowed_restrictions(player, apply_steal_allowed_restriction(player, mask))));
}

ActionMask GameState::apply_steal_allowed_restriction(int player, ActionMask mask) const {
    if (!steal_allowed_restricted_[player]) return mask;
    mask &= ~action_bit(Action::Income);
    mask &= ~action_bit(Action::ForeignAid);
    mask &= ~action_bit(Action::Steal);
    return mask;
}

ActionMask GameState::apply_block_allowed_restrictions(int player, ActionMask mask) const {
    if (foreign_aid_block_allowed_restricted_[player]) {
        mask &= ~action_bit(Action::ForeignAid);
    }
    if (steal_block_allowed_restricted_[player]) {
        mask &= ~action_bit(Action::Steal);
    }
    if (assassinate_block_allowed_restricted_[player]) {
        mask &= ~action_bit(Action::Assassinate);
    }
    return mask;
}

ActionMask GameState::apply_duke_claim_restriction(int player, ActionMask mask) const {
    if (!duke_claimed_[player]) return mask;
    if (live_count(player) != 2 || live_count(opponent(player)) != 2) return mask;
    mask &= ~action_bit(Action::Income);
    mask &= ~action_bit(Action::ForeignAid);
    return mask;
}

ActionMask GameState::apply_last_turn_action_restriction(int player, ActionMask mask) const {
    if (last_turn_action_[player] == Action::Tax) {
        mask &= ~action_bit(Action::Income);
        mask &= ~action_bit(Action::ForeignAid);
    } else if (last_turn_action_[player] == Action::ForeignAid) {
        mask &= ~action_bit(Action::Income);
    }
    return mask;
}

bool GameState::can_claim_mate(int player, const std::array<int, kPlayers>& coins) const {
    const int other = opponent(player);
    const int my_coins = coins[player];
    const int opp_coins = coins[other];
    const bool has_duke = has_live_card(player, Card::Duke);
    const bool has_contessa = has_live_card(player, Card::Contessa);
    const bool has_steal_blocker = has_live_card(player, Card::Captain) ||
                                   has_live_card(player, Card::Ambassador);

    if (live_count(player) == 1 && live_count(other) == 1) {
        return my_coins == 6 && opp_coins <= 2 && (has_duke || has_steal_blocker);
    }

    if (live_count(player) != 2 || live_count(other) != 1) return false;

    if (has_duke) {
        if (my_coins >= 4 && my_coins <= 6) return true;
        if (my_coins == 3 && opp_coins <= 5) return true;
        if (my_coins == 2 && opp_coins <= 3) return true;
        if (my_coins == 1 && opp_coins <= 1) return true;
    }

    if (has_duke && has_contessa) {
        if (my_coins == 3 && opp_coins <= 9) return true;
        if (my_coins == 2 && opp_coins <= 7) return true;
        if (my_coins == 1 && opp_coins <= 5) return true;
        if (my_coins == 0 && opp_coins <= 3) return true;
    }

    if (has_duke && has_steal_blocker && my_coins <= 1 && opp_coins <= 2) return true;

    if (has_steal_blocker) {
        if (my_coins == 6) return true;
        if (my_coins == 5 && opp_coins <= 5) return true;
        if (my_coins == 4 && opp_coins <= 2) return true;
    }

    if (has_steal_blocker && has_contessa) {
        if (my_coins == 5 && opp_coins <= 9) return true;
        if (my_coins == 4 && opp_coins <= 6) return true;
        if (my_coins == 3 && opp_coins <= 3) return true;
        if (my_coins == 2 && opp_coins == 0) return true;
    }

    return false;
}

ActionMask GameState::response_to_action_mask() const {
    ActionMask mask = action_allows_implicit_turn_continuation(pending_.action)
        ? turn_action_mask_for(current_player_, coins_after_primary_effect(pending_.action))
        : action_bit(Action::Allow);
    switch (pending_.action) {
        case Action::ForeignAid:
            mask &= ~action_bit(Action::Tax);
            mask |= action_bit(Action::BlockForeignAidDuke);
            break;
        case Action::Tax:
            mask &= ~action_bit(Action::ForeignAid);
            mask |= action_bit(Action::Challenge);
            break;
        case Action::Exchange:
            mask |= action_bit(Action::Challenge);
            break;
        case Action::Steal:
            mask &= ~action_bit(Action::Income);
            mask &= ~action_bit(Action::ForeignAid);
            mask &= ~action_bit(Action::Steal);
            mask |= action_bit(Action::Challenge);
            if (steal_allowed_restricted_[current_player_]) {
                break;
            }
            if (has_live_card(current_player_, Card::Captain) ||
                has_live_card(current_player_, Card::Ambassador)) {
                ActionMask block_mask = action_bit(Action::Challenge);
                if (has_live_card(current_player_, Card::Captain)) {
                    block_mask |= action_bit(Action::BlockStealCaptain);
                }
                if (has_live_card(current_player_, Card::Ambassador)) {
                    block_mask |= action_bit(Action::BlockStealAmbassador);
                }
                return block_mask;
            }
            mask |= action_bit(Action::BlockStealCaptain) | action_bit(Action::BlockStealAmbassador);
            break;
        case Action::Assassinate:
            if (has_live_card(current_player_, Card::Contessa)) {
                return action_bit(Action::BlockAssassinateContessa);
            }
            if (live_count(current_player_) == 1) {
                return action_bit(Action::Challenge) | action_bit(Action::BlockAssassinateContessa);
            }
            mask |= action_bit(Action::Challenge) | action_bit(Action::BlockAssassinateContessa);
            break;
        default:
            break;
    }
    return mask;
}

ActionMask GameState::response_to_block_mask() const {
    return action_bit(Action::Allow) | action_bit(Action::Challenge);
}

ActionMask GameState::challenge_reveal_mask() const {
    ActionMask mask = 0;
    const int player = pending_.challenged_player;
    const Card claim = pending_.claim_card;
    for (int slot = 0; slot < kInfluence; ++slot) {
        if (!live_[player][slot]) continue;
        if (cards_[player][slot] == claim) {
            mask |= action_bit(slot == 0 ? Action::RevealSlot0 : Action::RevealSlot1);
        }
    }
    if (mask != 0) return mask;
    return lose_influence_mask();
}

ActionMask GameState::lose_influence_mask() const {
    ActionMask mask = 0;
    const int player = current_player_;
    if (live_[player][0] && live_[player][1] && cards_[player][0] == cards_[player][1]) {
        return action_bit(Action::LoseSlot0);
    }
    if (live_[player][0]) mask |= action_bit(Action::LoseSlot0);
    if (live_[player][1]) mask |= action_bit(Action::LoseSlot1);
    return mask;
}

ActionMask GameState::exchange_choose_mask() const {
    const int live_cards = live_count(current_player_);
    ActionMask mask = 0;
    if (live_cards == 1) {
        for (int i = 0; i < 4; ++i) {
            if (exchange_cards_[i] == Card::None) continue;
            mask |= action_bit(static_cast<Action>(static_cast<int>(Action::Keep0) + i));
        }
    } else {
        constexpr std::array<Action, 6> pairs = {
            Action::Keep01, Action::Keep02, Action::Keep03,
            Action::Keep12, Action::Keep13, Action::Keep23,
        };
        for (Action action : pairs) {
            const auto positions = keep_pair_positions(action);
            if (exchange_cards_[positions[0]] != Card::None && exchange_cards_[positions[1]] != Card::None) {
                mask |= action_bit(action);
            }
        }
    }
    return mask;
}

void GameState::apply_turn_action(Action action) {
    append_event(action, current_player_);
    pending_ = Pending{};
    pending_.action = action;
    pending_.actor = current_player_;
    pending_.target = opponent(current_player_);
    last_turn_action_[current_player_] = action;
    if (action == Action::Tax) {
        duke_claimed_[current_player_] = true;
    }
    if (action == Action::Income) {
        ++coins_[current_player_];
        end_turn_after_action();
    } else if (action == Action::Coup) {
        coins_[current_player_] -= kCoupCost;
        assert(coins_[current_player_] >= 0);
        start_loss(pending_.target);
    } else if (action == Action::ClaimMate) {
        live_[pending_.target][0] = false;
        live_[pending_.target][1] = false;
        phase_ = Phase::Terminal;
    } else if (action == Action::Assassinate) {
        coins_[current_player_] -= kAssassinateCost;
        assert(coins_[current_player_] >= 0);
        current_player_ = pending_.target;
        phase_ = Phase::ResponseToAction;
    } else {
        current_player_ = pending_.target;
        phase_ = Phase::ResponseToAction;
    }
}

void GameState::apply_action_response(Action action) {
    if (action == Action::Allow) {
        append_event(action, current_player_);
        continue_successful_action();
    } else if (action == Action::Challenge) {
        append_event(action, current_player_);
        start_challenge(current_player_, pending_.actor, ClaimKind::Action,
                        primary_claim_card(pending_.action));
    } else if (block_claim_card(action) != Card::None) {
        append_event(action, current_player_);
        if (action == Action::BlockForeignAidDuke) {
            duke_claimed_[current_player_] = true;
        }
        pending_.block_action = action;
        pending_.block_actor = current_player_;
        pending_.block_card = block_claim_card(action);
        if (pending_.action == Action::Steal) {
            steal_block_allowed_restricted_[current_player_] = true;
        }
        current_player_ = pending_.actor;
        phase_ = Phase::ResponseToBlock;
    } else {
        apply_implicit_allow_continuation(action);
    }
}

void GameState::apply_block_response(Action action) {
    append_event(action, current_player_);
    if (action == Action::Allow) {
        if (pending_.action == Action::ForeignAid) {
            foreign_aid_block_allowed_restricted_[pending_.actor] = true;
        } else if (pending_.action == Action::Steal) {
            steal_block_allowed_restricted_[pending_.actor] = true;
        } else if (pending_.action == Action::Assassinate) {
            assassinate_block_allowed_restricted_[pending_.actor] = true;
        }
        end_turn_after_action();
    } else {
        start_challenge(current_player_, pending_.block_actor, ClaimKind::Block, pending_.block_card);
    }
}

void GameState::apply_challenge_reveal(Action action) {
    const int slot = slot_from_reveal_action(action);
    if (slot >= 0) {
        append_event(action, current_player_, pending_.claim_card);
        pending_.challenge_truthful = true;
        pending_.challenge_loser = pending_.challenger;
        pending_.revealed_slot = slot;
        deck_[card_index(cards_[pending_.challenged_player][slot])]++;
        start_loss(pending_.challenge_loser);
        return;
    }

    pending_.challenge_truthful = false;
    pending_.challenge_loser = pending_.challenged_player;
    apply_lose_influence(action);
}

void GameState::apply_lose_influence(Action action) {
    const int slot = slot_from_loss_action(action);
    assert(slot >= 0);
    const int player = current_player_;
    append_event(action, player, cards_[player][slot]);
    live_[player][slot] = false;
    if (post_2v2_start_history_len_ < 0 && !is_2v2()) {
        post_2v2_start_history_len_ = public_history_len_;
    }
    steal_allowed_restricted_ = {false, false};
    foreign_aid_block_allowed_restricted_ = {false, false};
    steal_block_allowed_restricted_ = {false, false};
    assassinate_block_allowed_restricted_ = {false, false};
    finish_loss();
}

void GameState::apply_exchange_choose(Action action) {
    append_event(action, current_player_);
    apply_keep_choice(action);
    append_public_observation(public_exchange_complete_token(current_player_));
    end_turn_after_action();
}

void GameState::start_challenge(int challenger, int challenged, ClaimKind claim_kind, Card claim_card) {
    pending_.challenger = challenger;
    pending_.challenged_player = challenged;
    pending_.claim_kind = claim_kind;
    pending_.claim_card = claim_card;
    current_player_ = challenged;
    phase_ = Phase::ChallengeReveal;
}

void GameState::start_loss(int player) {
    current_player_ = player;
    phase_ = Phase::LoseInfluence;
}

void GameState::finish_loss() {
    if (live_count(current_player_) == 0) {
        phase_ = Phase::Terminal;
        return;
    }

    if (pending_.challenge_loser >= 0) {
        if (pending_.challenge_truthful) {
            phase_ = Phase::Redraw;
            current_player_ = pending_.challenged_player;
        } else if (pending_.claim_kind == ClaimKind::Action) {
            end_turn_after_action();
        } else {
            continue_after_failed_block();
        }
        return;
    }

    end_turn_after_action();
}

void GameState::finish_truthful_challenge_after_redraw() {
    if (pending_.claim_kind == ClaimKind::Action) {
        continue_successful_action();
    } else {
        end_turn_after_action();
    }
}

void GameState::continue_successful_action() {
    switch (pending_.action) {
        case Action::ForeignAid:
        case Action::Tax:
        case Action::Steal:
            apply_primary_effect(pending_.action);
            end_turn_after_action();
            break;
        case Action::Exchange:
            phase_ = Phase::ExchangeDraw;
            current_player_ = pending_.actor;
            break;
        case Action::Assassinate:
            start_loss(pending_.target);
            break;
        default:
            assert(false && "invalid successful pending action");
            break;
    }
}

void GameState::continue_after_failed_block() {
    if (pending_.action == Action::Assassinate) {
        start_loss(pending_.target);
    } else {
        apply_primary_effect(pending_.action);
        end_turn_after_action();
    }
}

void GameState::end_turn_after_action() {
    set_terminal_or_next_turn(opponent(pending_.actor));
}

void GameState::set_terminal_or_next_turn(int next_player) {
    if (live_count(0) == 0 || live_count(1) == 0) {
        phase_ = Phase::Terminal;
        return;
    }
    current_player_ = next_player;
    phase_ = Phase::TurnAction;
    pending_ = Pending{};
    exchange_cards_.fill(Card::None);
}

int GameState::opponent(int player) const {
    return 1 - player;
}

int GameState::live_count(int player) const {
    return (live_[player][0] ? 1 : 0) + (live_[player][1] ? 1 : 0);
}

bool GameState::has_live_card(int player, Card card) const {
    return (live_[player][0] && cards_[player][0] == card) ||
           (live_[player][1] && cards_[player][1] == card);
}

int GameState::slot_from_reveal_action(Action action) const {
    if (action == Action::RevealSlot0) return 0;
    if (action == Action::RevealSlot1) return 1;
    return -1;
}

int GameState::slot_from_loss_action(Action action) const {
    if (action == Action::LoseSlot0) return 0;
    if (action == Action::LoseSlot1) return 1;
    return -1;
}

Card GameState::primary_claim_card(Action action) const {
    switch (action) {
        case Action::Tax: return Card::Duke;
        case Action::Steal: return Card::Captain;
        case Action::Exchange: return Card::Ambassador;
        case Action::Assassinate: return Card::Assassin;
        default: return Card::None;
    }
}

Card GameState::block_claim_card(Action action) const {
    switch (action) {
        case Action::BlockForeignAidDuke: return Card::Duke;
        case Action::BlockStealCaptain: return Card::Captain;
        case Action::BlockStealAmbassador: return Card::Ambassador;
        case Action::BlockAssassinateContessa: return Card::Contessa;
        default: return Card::None;
    }
}

bool GameState::action_is_primary_claim(Action action) const {
    return primary_claim_card(action) != Card::None;
}

int GameState::steal_amount() const {
    return std::min(2, coins_[pending_.target]);
}

void GameState::apply_primary_effect(Action action) {
    switch (action) {
        case Action::ForeignAid:
            coins_[pending_.actor] += 2;
            break;
        case Action::Tax:
            coins_[pending_.actor] += 3;
            break;
        case Action::Steal: {
            const int amount = steal_amount();
            coins_[pending_.target] -= amount;
            coins_[pending_.actor] += amount;
            break;
        }
        default:
            assert(false && "unsupported primary effect");
            break;
    }
}

std::array<int, kPlayers> GameState::coins_after_primary_effect(Action action) const {
    std::array<int, kPlayers> coins = coins_;
    switch (action) {
        case Action::ForeignAid:
            coins[pending_.actor] += 2;
            break;
        case Action::Tax:
            coins[pending_.actor] += 3;
            break;
        case Action::Steal: {
            const int amount = std::min(2, coins[pending_.target]);
            coins[pending_.target] -= amount;
            coins[pending_.actor] += amount;
            break;
        }
        default:
            break;
    }
    return coins;
}

bool GameState::action_allows_implicit_turn_continuation(Action action) const {
    return action == Action::ForeignAid || action == Action::Tax || action == Action::Steal;
}

void GameState::apply_implicit_allow_continuation(Action turn_action) {
    assert(action_allows_implicit_turn_continuation(pending_.action));
    const int responder = current_player_;
    apply_primary_effect(pending_.action);
    if (pending_.action == Action::Steal) {
        steal_allowed_restricted_[responder] = true;
    }
    current_player_ = responder;
    phase_ = Phase::TurnAction;
    pending_ = Pending{};
    apply_turn_action(turn_action);
}

void GameState::apply_keep_choice(Action action) {
    const int actor = current_player_;
    std::array<Card, 2> keep{Card::None, Card::None};
    int keep_count = 0;
    if (is_keep_single(action)) {
        keep[keep_count++] = exchange_cards_[keep_single_position(action)];
    } else if (is_keep_pair(action)) {
        const auto positions = keep_pair_positions(action);
        keep[keep_count++] = exchange_cards_[positions[0]];
        keep[keep_count++] = exchange_cards_[positions[1]];
    } else {
        assert(false && "invalid keep action");
    }

    for (Card card : exchange_cards_) {
        if (card != Card::None) ++deck_[card_index(card)];
    }
    for (int i = 0; i < keep_count; ++i) {
        --deck_[card_index(keep[i])];
    }

    int next_keep = 0;
    std::array<Card, 2> final_cards{Card::None, Card::None};
    for (int slot = 0; slot < kInfluence; ++slot) {
        if (!live_[actor][slot]) continue;
        assert(next_keep < keep_count);
        cards_[actor][slot] = keep[next_keep++];
        final_cards[slot] = cards_[actor][slot];
    }
    append_private_observation(actor, private_exchange_keep_token(actor, action, final_cards[0], final_cards[1]));
    exchange_cards_.fill(Card::None);
}

} // namespace coup
