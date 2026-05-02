#include "observation.hpp"

#include "game_state.hpp"

#include <stdexcept>

namespace coup {

namespace {

uint8_t as_u8(Card card) {
    return static_cast<uint8_t>(card);
}

uint8_t as_u8(Action action) {
    return static_cast<uint8_t>(action);
}

std::size_t mix_hash(std::size_t seed, std::size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

ObservationToken token(ObservationTokenKind kind, int player) {
    ObservationToken out;
    out.kind = kind;
    out.player = static_cast<uint8_t>(player);
    return out;
}

} // namespace

bool ObservationToken::operator==(const ObservationToken& other) const {
    return kind == other.kind &&
           player == other.player &&
           action == other.action &&
           card0 == other.card0 &&
           card1 == other.card1 &&
           card2 == other.card2 &&
           card3 == other.card3;
}

ObservationStore::ObservationStore() {
    nodes_.push_back(ObservationNode{0, ObservationToken{}});
}

uint32_t ObservationStore::append(uint32_t parent, const ObservationToken& token_value) {
    if (parent >= nodes_.size()) throw std::invalid_argument("invalid observation parent");
    const EntryKey key{parent, token_value};
    const auto found = ids_.find(key);
    if (found != ids_.end()) return found->second;

    const uint32_t id = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back(ObservationNode{parent, token_value});
    ids_.emplace(key, id);
    return id;
}

const ObservationNode& ObservationStore::node(uint32_t id) const {
    if (id >= nodes_.size()) throw std::invalid_argument("invalid observation id");
    return nodes_[id];
}

std::size_t ObservationStore::size() const {
    return nodes_.size();
}

bool ObservationStore::EntryKey::operator==(const EntryKey& other) const {
    return parent == other.parent && token == other.token;
}

std::size_t ObservationStore::EntryKeyHash::operator()(const EntryKey& key) const {
    std::size_t seed = key.parent;
    seed = mix_hash(seed, static_cast<std::size_t>(key.token.kind));
    seed = mix_hash(seed, key.token.player);
    seed = mix_hash(seed, key.token.action);
    seed = mix_hash(seed, key.token.card0);
    seed = mix_hash(seed, key.token.card1);
    seed = mix_hash(seed, key.token.card2);
    seed = mix_hash(seed, key.token.card3);
    return seed;
}

ObservationStorePtr make_observation_store() {
    return std::make_shared<ObservationStore>();
}

ObservationToken initial_cards_token(int player, Card card0, Card card1) {
    ObservationToken out = token(ObservationTokenKind::InitialCards, player);
    out.card0 = as_u8(card0);
    out.card1 = as_u8(card1);
    return out;
}

ObservationToken public_action_token(int player, Action action) {
    ObservationToken out = token(ObservationTokenKind::PublicAction, player);
    out.action = as_u8(action);
    return out;
}

ObservationToken public_reveal_token(int player, Action action, Card card) {
    ObservationToken out = token(ObservationTokenKind::PublicReveal, player);
    out.action = as_u8(action);
    out.card0 = as_u8(card);
    return out;
}

ObservationToken public_lose_token(int player, Action action, Card card) {
    ObservationToken out = token(ObservationTokenKind::PublicLose, player);
    out.action = as_u8(action);
    out.card0 = as_u8(card);
    return out;
}

ObservationToken public_replacement_token(int player) {
    return token(ObservationTokenKind::PublicReplacement, player);
}

ObservationToken public_exchange_complete_token(int player) {
    return token(ObservationTokenKind::PublicExchangeComplete, player);
}

ObservationToken private_replacement_token(int player, Card card) {
    ObservationToken out = token(ObservationTokenKind::PrivateReplacement, player);
    out.card0 = as_u8(card);
    return out;
}

ObservationToken private_exchange_draw_token(int player, Card first, Card second) {
    ObservationToken out = token(ObservationTokenKind::PrivateExchangeDraw, player);
    out.card0 = as_u8(first);
    out.card1 = as_u8(second);
    return out;
}

ObservationToken private_exchange_keep_token(int player, Action keep_action, Card card0, Card card1) {
    ObservationToken out = token(ObservationTokenKind::PrivateExchangeKeep, player);
    out.action = as_u8(keep_action);
    out.card0 = as_u8(card0);
    out.card1 = as_u8(card1);
    return out;
}

} // namespace coup
