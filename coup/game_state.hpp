#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────
//  Action enum  (27 actions, unchanged)
// ─────────────────────────────────────────────
const char* const ACTION_NAMES[] = {
    "INCOME", "FOREIGN_AID", "TAX", "STEAL1", "STEAL2", "ASSASSINATE", "COUP",
    "BLOCK_FOREIGN_AID", "BLOCK_STEAL1_AMB", "BLOCK_STEAL2_AMB",
    "BLOCK_STEAL1_CAP", "BLOCK_STEAL2_CAP", "BLOCK_ASSASSINATE",
    "CHALLENGE", "PASS_BLOCK",
    "SHOW_ASSASSIN", "SHOW_AMBASSADOR", "SHOW_CAPTAIN", "SHOW_CONTESSA", "SHOW_DUKE",
    "LOSE_ASSASSIN", "LOSE_AMBASSADOR", "LOSE_CAPTAIN", "LOSE_CONTESSA", "LOSE_DUKE",
    "LOSE_BOTH", "CLAIM_MATE"
};

enum Action {
    INCOME = 0,
    FOREIGN_AID = 1,
    TAX = 2,
    STEAL1 = 3,
    STEAL2 = 4,
    ASSASSINATE = 5,
    COUP = 6,
    BLOCK_FOREIGN_AID = 7,
    BLOCK_STEAL1_AMB = 8,
    BLOCK_STEAL2_AMB = 9,
    BLOCK_STEAL1_CAP = 10,
    BLOCK_STEAL2_CAP = 11,
    BLOCK_ASSASSINATE = 12,
    CHALLENGE = 13,
    PASS_BLOCK = 14,
    SHOW_ASSASSIN = 15,
    SHOW_AMBASSADOR = 16,
    SHOW_CAPTAIN = 17,
    SHOW_CONTESSA = 18,
    SHOW_DUKE = 19,
    LOSE_ASSASSIN = 20,
    LOSE_AMBASSADOR = 21,
    LOSE_CAPTAIN = 22,
    LOSE_CONTESSA = 23,
    LOSE_DUKE = 24,
    LOSE_BOTH = 25,
    CLAIM_MATE = 26
};

enum Card { ASSASSIN, AMBASSADOR, CAPTAIN, CONTESSA, DUKE };

// ─────────────────────────────────────────────
//  Phase enum  (new)
//
//  Each value names who acts and why.
//  current_player is always set to match.
//
//  ACTION            active_player picks their action
//  RESPOND           opponent may: allow (next action), block, or challenge
//  CHALLENGE_BLOCK   active_player may: challenge the block, or pass
//  SHOW_CARD         challenged player reveals (or admits bluff → LOSE_CARD)
//  LOSE_CARD         loser of challenge / target of coup / target of
//                    assassination chooses which influence to discard
//  TERMINAL          game over
// ─────────────────────────────────────────────
enum class Phase {
    ACTION,
    RESPOND,
    CHALLENGE_BLOCK,
    SHOW_CARD,
    LOSE_CARD,
    TERMINAL
};

struct PhaseSnapshot {
    Phase  phase;
    int    active_player;
    int    current_player;
    Action pending_action;
};

// ─────────────────────────────────────────────
//  Rules config  (unchanged)
// ─────────────────────────────────────────────
struct BaselineRulesConfig {
    bool exchange_enabled    = false;
    bool reveal_redraw_enabled = false;
};

struct PruningRulesConfig {
    bool enabled = true;
};

struct ExtensionRulesConfig {
    bool claim_mate_enabled = true;
};

struct RulesConfig {
    BaselineRulesConfig  baseline{};
    PruningRulesConfig   pruning{};
    ExtensionRulesConfig extensions{};

    static RulesConfig solver_default();
    static RulesConfig baseline_default();
};

// ─────────────────────────────────────────────
//  Card holdings  (unchanged)
// ─────────────────────────────────────────────
const std::array<std::array<Card, 2>, 15> holdings = {{
    {ASSASSIN,  ASSASSIN},
    {ASSASSIN,  AMBASSADOR},
    {ASSASSIN,  CAPTAIN},
    {ASSASSIN,  CONTESSA},
    {ASSASSIN,  DUKE},
    {AMBASSADOR,AMBASSADOR},
    {AMBASSADOR,CAPTAIN},
    {AMBASSADOR,CONTESSA},
    {AMBASSADOR,DUKE},
    {CAPTAIN,   CAPTAIN},
    {CAPTAIN,   CONTESSA},
    {CAPTAIN,   DUKE},
    {CONTESSA,  CONTESSA},
    {CONTESSA,  DUKE},
    {DUKE,      DUKE}
}};

const int NUM_ACTIONS   = 27;
const int NUM_HOLDINGS  = 15;
const int COIN_TO_ASSASSINATE = 3;
const int COIN_TO_COUP        = 7;
const int COIN_TO_MUST_COUP   = 10;

using ActionMask = uint32_t;

// ─────────────────────────────────────────────
//  GameState
// ─────────────────────────────────────────────
class GameState {
public:
    // ── configuration ──
    RulesConfig rules_config;

    // ── phase / turn ownership ──
    Phase phase;
    int   active_player;   // whose "turn" it is (stable across a full turn sequence)
    int   current_player;  // who acts right now (changes each phase transition)

    // ── pending action (replaces history-walking for challenge resolution) ──
    // Set to the action currently under challenge or awaiting response.
    // Avoids history[size-2], history[size-3] lookups scattered through the code.
    Action pending_action;

    // ── cards & resources ──
    std::array<Card, 2> p1_cards;
    std::array<Card, 2> p2_cards;
    std::array<int,  2> p1_influence;
    std::array<int,  2> p2_influence;
    int p1_coins;
    int p2_coins;

    // ── history (kept for infoset hashing & CFR) ──
    std::vector<Action> history;

    // ── pruning: accepted-action counters ──
    int num_p1_has_allowed_tax;
    int num_p2_has_allowed_tax;
    int num_p1_has_allowed_block_fa;
    int num_p2_has_allowed_block_fa;
    int num_p1_has_allowed_foreign_aid;
    int num_p2_has_allowed_foreign_aid;
    int num_p1_has_allowed_steal;
    int num_p2_has_allowed_steal;
    int num_p1_has_allowed_assassinate;
    int num_p2_has_allowed_assassinate;

    // ── pruning: public claim counters ──
    int num_p1_has_claimed_duke;
    int num_p2_has_claimed_duke;
    int num_p1_has_claimed_steal_blocker;
    int num_p2_has_claimed_steal_blocker;
    int num_p1_has_claimed_contessa;
    int num_p2_has_claimed_contessa;

    // ── pruning: snapshots at first influence loss ──
    int p1_claims_duke_at_first_loss;
    int p1_claims_steal_blocker_at_first_loss;
    int p1_claims_contessa_at_first_loss;
    int p2_claims_duke_at_first_loss;
    int p2_claims_steal_blocker_at_first_loss;
    int p2_claims_contessa_at_first_loss;

    // ── construction ──
    GameState();
    explicit GameState(const RulesConfig&);
    void reset();

    // ── queries ──
    bool   is_terminal() const;
    double get_utility() const;
    double get_br_utility(int max_player) const;
    int    get_current_player() const;

    // ── configuration ──
    void              set_rules_config(const RulesConfig&);
    const RulesConfig& get_rules_config() const;

    // ── card setup ──
    void set_cards(Card, Card, Card, Card);
    void set_my_cards(std::array<Card, 2>);

    // ── legal actions ──
    std::vector<Action> get_legal_actions() const;

    // ── do / undo ──
    void do_action(Action);
    void undo_action();

    // ── hashing ──
    size_t get_history_hash() const;
    static size_t get_history_hash(const std::vector<Action>&);
    size_t get_infoset_hash() const;
    std::string get_infoset_string() const;

    // ── pruning helpers (used by Trainer) ──
    bool has_allowed_foreign_aid()  const;
    bool has_allowed_steal()        const;
    bool has_allowed_assassinate()  const;
    bool has_opponent_allowed_tax()         const;
    bool has_opponent_allowed_foreign_aid() const;
    bool has_opponent_allowed_steal()       const;
    bool has_opponent_claimed_duke_2v2(bool is_p1)          const;
    bool has_opponent_claimed_steal_blocker_2v2(bool is_p1) const;
    bool has_opponent_claimed_contessa_2v2(bool is_p1)      const;
    bool has_opponent_claimed_duke_xv1(bool is_p1)          const;
    bool has_opponent_claimed_steal_blocker_xv1(bool is_p1) const;
    bool has_opponent_claimed_contessa_xv1(bool is_p1)      const;

    // ── extension helper ──
    bool can_2v1_coupmate(int my_coins, int opp_coins, const std::array<Card, 2>& my_cards) const;

    // ── card-loss helpers ──
    std::vector<Action> get_card_losing_actions(const std::array<Card, 2>&,
                                                 const std::array<int, 2>&) const;
    void set_card_losing_bits(const std::array<Card, 2>&,
                               const std::array<int, 2>&,
                               ActionMask&) const;

    // ── debugging ──
    void        print_history()    const;
    void        print_game_state() const;
    std::string get_game_state()   const;

private:
    std::vector<PhaseSnapshot> phase_stack;

    // ── legal-action building (one method per phase) ──
    ActionMask legal_mask_action()          const;
    ActionMask legal_mask_respond()         const;
    ActionMask legal_mask_challenge_block() const;
    ActionMask legal_mask_show_card()       const;
    ActionMask legal_mask_lose_card()       const;

    ActionMask apply_pruning_heuristics(ActionMask) const;
    ActionMask add_extension_actions(ActionMask)    const;
    std::vector<Action> actions_from_mask(ActionMask) const;

    // ── phase transition helpers ──
    // Called at the end of do_action / undo_action to update phase, active_player,
    // current_player, and pending_action consistently.
    void advance_phase(Action);
    void retreat_phase(Action);   // inverse of advance_phase, used by undo_action

    // ── coin / influence mutation ──
    void apply_action_effects(Action);
    void undo_action_effects(Action);

    // ── lose-card helpers ──
    void lose_card(Card);
    void undo_lose_card(Card);

    // ── pruning helpers ──
    bool can_force_coup_vs_one_influence() const;
    bool can_force_claim_mate()            const;
    bool is_free_turn()                    const;

    // ── pruning tracker updates ──
    void update_claim_trackers_on_action(Action);
    void update_allow_trackers_on_action(Action);
    void undo_claim_trackers_on_action(Action);
    void undo_allow_trackers_on_action(Action);

    void add_primary_turn_actions(ActionMask&, int my_coins, int opp_coins) const;
};

#endif // GAME_STATE_HPP
