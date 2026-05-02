#ifndef COUP_GAME_STATE_HPP
#define COUP_GAME_STATE_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "observation.hpp"

namespace coup {

enum class Card : uint8_t {
    Duke = 0,
    Assassin = 1,
    Captain = 2,
    Ambassador = 3,
    Contessa = 4,
    None = 5,
};

enum class Phase : uint8_t {
    TurnAction = 0,
    ResponseToAction = 1,
    ResponseToBlock = 2,
    ChallengeReveal = 3,
    LoseInfluence = 4,
    Redraw = 5,
    ExchangeDraw = 6,
    ExchangeChoose = 7,
    Terminal = 8,
};

enum class Action : uint8_t {
    Income = 0,
    ForeignAid = 1,
    Tax = 2,
    Steal = 3,
    Exchange = 4,
    Assassinate = 5,
    Coup = 6,
    Allow = 7,
    Challenge = 8,
    BlockForeignAidDuke = 9,
    BlockStealCaptain = 10,
    BlockStealAmbassador = 11,
    BlockAssassinateContessa = 12,
    RevealSlot0 = 13,
    RevealSlot1 = 14,
    LoseSlot0 = 15,
    LoseSlot1 = 16,
    Keep0 = 17,
    Keep1 = 18,
    Keep2 = 19,
    Keep3 = 20,
    Keep01 = 21,
    Keep02 = 22,
    Keep03 = 23,
    Keep12 = 24,
    Keep13 = 25,
    Keep23 = 26,
    ClaimMate = 27,
    Count = 28,
};

enum class ChanceType : uint8_t {
    Redraw = 0,
    ExchangeDraw = 1,
};

using ActionMask = uint64_t;

constexpr int kPlayers = 2;
constexpr int kInfluence = 2;
constexpr int kCardTypes = 5;
constexpr int kStartingCoins = 2;
constexpr int kAssassinateCost = 3;
constexpr int kCoupCost = 7;
constexpr int kForcedCoupCoins = 10;
constexpr int kMaxPublicHistory = 128;

struct Deal {
    std::array<std::array<Card, kInfluence>, kPlayers> cards{};
};

struct ChanceOutcome {
    ChanceType type{};
    Card first{Card::None};
    Card second{Card::None};
    double probability{0.0};
};

struct PublicEvent {
    Action action{Action::Count};
    int player{-1};
    Card card{Card::None};
};

struct InfosetKey {
    uint32_t public_obs_id{0};
    uint32_t private_obs_id{0};

    bool operator==(const InfosetKey& other) const;
};

class GameState {
public:
    GameState();
    explicit GameState(const Deal& deal);
    explicit GameState(ObservationStorePtr observation_store);
    GameState(const Deal& deal, ObservationStorePtr observation_store);

    void reset();
    void reset(const Deal& deal);
    void set_deal(const Deal& deal);

    bool is_terminal() const;
    int current_player() const;
    Phase phase() const;
    ActionMask legal_actions() const;
    bool is_legal(Action action) const;

    void apply(Action action);
    void undo();

    bool is_chance_node() const;
    std::vector<ChanceOutcome> chance_outcomes() const;
    void apply_chance(const ChanceOutcome& outcome);
    void undo_chance();

    double utility(int player) const;
    InfosetKey infoset(int player) const;
    uint32_t public_observation_id() const;
    uint32_t private_observation_id(int player) const;

    int coins(int player) const;
    bool live(int player, int slot) const;
    Card card(int player, int slot) const;
    int deck_count(Card card) const;
    int public_history_size() const;
    bool is_2v2() const;
    int post_2v2_public_history_size() const;
    const PublicEvent& public_event(int index) const;

    std::string debug_string() const;
    std::string infoset_debug_string(int player) const;

private:
    enum class ClaimKind : uint8_t {
        None = 0,
        Action = 1,
        Block = 2,
    };

    struct Pending {
        Action action{Action::Count};
        int actor{-1};
        int target{-1};
        ClaimKind claim_kind{ClaimKind::None};
        Card claim_card{Card::None};
        Action block_action{Action::Count};
        int block_actor{-1};
        Card block_card{Card::None};
        int challenged_player{-1};
        int challenger{-1};
        int challenge_loser{-1};
        bool challenge_truthful{false};
        int revealed_slot{-1};
    };

    struct Snapshot {
        Phase phase{};
        int current_player{};
        std::array<int, kPlayers> coins{};
        std::array<std::array<Card, kInfluence>, kPlayers> cards{};
        std::array<std::array<bool, kInfluence>, kPlayers> live{};
        std::array<bool, kPlayers> steal_allowed_restricted{};
        std::array<bool, kPlayers> foreign_aid_block_allowed_restricted{};
        std::array<bool, kPlayers> steal_block_allowed_restricted{};
        std::array<bool, kPlayers> assassinate_block_allowed_restricted{};
        std::array<bool, kPlayers> duke_claimed{};
        std::array<Action, kPlayers> last_turn_action{};
        std::array<int, kCardTypes> deck{};
        Pending pending{};
        std::array<Card, 4> exchange_cards{};
        int public_history_len{};
        int post_2v2_start_history_len{};
        uint32_t public_obs_id{};
        std::array<uint32_t, kPlayers> private_obs_id{};
    };

    Phase phase_{Phase::TurnAction};
    int current_player_{0};
    std::array<int, kPlayers> coins_{};
    std::array<std::array<Card, kInfluence>, kPlayers> cards_{};
    std::array<std::array<bool, kInfluence>, kPlayers> live_{};
    std::array<bool, kPlayers> steal_allowed_restricted_{false, false};
    std::array<bool, kPlayers> foreign_aid_block_allowed_restricted_{false, false};
    std::array<bool, kPlayers> steal_block_allowed_restricted_{false, false};
    std::array<bool, kPlayers> assassinate_block_allowed_restricted_{false, false};
    std::array<bool, kPlayers> duke_claimed_{false, false};
    std::array<Action, kPlayers> last_turn_action_{Action::Count, Action::Count};
    std::array<int, kCardTypes> deck_{};
    Pending pending_{};
    std::array<Card, 4> exchange_cards_{};
    int public_history_len_{0};
    int post_2v2_start_history_len_{-1};
    ObservationStorePtr observation_store_;
    uint32_t public_obs_id_{0};
    std::array<uint32_t, kPlayers> private_obs_id_{};
    std::array<PublicEvent, kMaxPublicHistory> public_history_{};
    std::vector<Snapshot> undo_stack_;

    Snapshot snapshot() const;
    void restore(const Snapshot& snapshot);
    void save_undo();

    void initialize_deck();
    void remove_dealt_cards();
    void initialize_observations();
    void append_event(Action action, int player, Card card = Card::None);
    void append_public_observation(const ObservationToken& token);
    void append_private_observation(int player, const ObservationToken& token);
    void append_public_action_observation(Action action, int player, Card card);

    ActionMask turn_action_mask() const;
    ActionMask turn_action_mask_for(int player, const std::array<int, kPlayers>& coins) const;
    ActionMask apply_steal_allowed_restriction(int player, ActionMask mask) const;
    ActionMask apply_block_allowed_restrictions(int player, ActionMask mask) const;
    ActionMask apply_duke_claim_restriction(int player, ActionMask mask) const;
    ActionMask apply_last_turn_action_restriction(int player, ActionMask mask) const;
    bool can_claim_mate(int player, const std::array<int, kPlayers>& coins) const;
    ActionMask response_to_action_mask() const;
    ActionMask response_to_block_mask() const;
    ActionMask challenge_reveal_mask() const;
    ActionMask lose_influence_mask() const;
    ActionMask exchange_choose_mask() const;

    void apply_turn_action(Action action);
    void apply_action_response(Action action);
    void apply_block_response(Action action);
    void apply_challenge_reveal(Action action);
    void apply_lose_influence(Action action);
    void apply_exchange_choose(Action action);

    void start_challenge(int challenger, int challenged, ClaimKind claim_kind, Card claim_card);
    void start_loss(int player);
    void finish_loss();
    void finish_truthful_challenge_after_redraw();
    void continue_successful_action();
    void continue_after_failed_block();
    void end_turn_after_action();
    void set_terminal_or_next_turn(int next_player);

    int opponent(int player) const;
    int live_count(int player) const;
    bool has_live_card(int player, Card card) const;
    int slot_from_reveal_action(Action action) const;
    int slot_from_loss_action(Action action) const;
    Card primary_claim_card(Action action) const;
    Card block_claim_card(Action action) const;
    bool action_is_primary_claim(Action action) const;
    int steal_amount() const;

    void apply_primary_effect(Action action);
    std::array<int, kPlayers> coins_after_primary_effect(Action action) const;
    bool action_allows_implicit_turn_continuation(Action action) const;
    void apply_implicit_allow_continuation(Action turn_action);
    void apply_keep_choice(Action action);

};

ActionMask action_bit(Action action);
const char* card_name(Card card);
const char* action_name(Action action);
const char* phase_name(Phase phase);

} // namespace coup

#endif
