#ifndef COUP_OBSERVATION_HPP
#define COUP_OBSERVATION_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coup {

enum class Card : uint8_t;
enum class Action : uint8_t;

enum class ObservationTokenKind : uint8_t {
    InitialCards = 0,
    PublicAction = 1,
    PublicReveal = 2,
    PublicLose = 3,
    PublicReplacement = 4,
    PublicExchangeComplete = 5,
    PrivateReplacement = 6,
    PrivateExchangeDraw = 7,
    PrivateExchangeKeep = 8,
};

struct ObservationToken {
    ObservationTokenKind kind{ObservationTokenKind::PublicAction};
    uint8_t player{0};
    uint8_t action{0};
    uint8_t card0{5};
    uint8_t card1{5};
    uint8_t card2{5};
    uint8_t card3{5};

    bool operator==(const ObservationToken& other) const;
};

struct ObservationNode {
    uint32_t parent{0};
    ObservationToken token{};
};

class ObservationStore {
public:
    ObservationStore();

    uint32_t append(uint32_t parent, const ObservationToken& token);
    const ObservationNode& node(uint32_t id) const;
    std::size_t size() const;

private:
    struct EntryKey {
        uint32_t parent{0};
        ObservationToken token{};

        bool operator==(const EntryKey& other) const;
    };

    struct EntryKeyHash {
        std::size_t operator()(const EntryKey& key) const;
    };

    std::vector<ObservationNode> nodes_;
    std::unordered_map<EntryKey, uint32_t, EntryKeyHash> ids_;
};

using ObservationStorePtr = std::shared_ptr<ObservationStore>;

ObservationStorePtr make_observation_store();

ObservationToken initial_cards_token(int player, Card card0, Card card1);
ObservationToken public_action_token(int player, Action action);
ObservationToken public_reveal_token(int player, Action action, Card card);
ObservationToken public_lose_token(int player, Action action, Card card);
ObservationToken public_replacement_token(int player);
ObservationToken public_exchange_complete_token(int player);
ObservationToken private_replacement_token(int player, Card card);
ObservationToken private_exchange_draw_token(int player, Card first, Card second);
ObservationToken private_exchange_keep_token(int player, Action keep_action, Card card0, Card card1);

} // namespace coup

#endif
