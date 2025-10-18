#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <array>
#include <cstddef> // for size_t
#include <string>
#include <vector>

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
  LOSE_BOTH = 25, // Double assassination
  CLAIM_MATE = 26 // Claiming Coup Mate
};

enum Card { ASSASSIN, AMBASSADOR, CAPTAIN, CONTESSA, DUKE };

const std::array<std::array<Card, 2>, 15> holdings = {{
    {ASSASSIN, ASSASSIN},
    {ASSASSIN, AMBASSADOR},
    {ASSASSIN, CAPTAIN},
    {ASSASSIN, CONTESSA},
    {ASSASSIN, DUKE},
    {AMBASSADOR, AMBASSADOR},
    {AMBASSADOR, CAPTAIN},
    {AMBASSADOR, CONTESSA},
    {AMBASSADOR, DUKE},
    {CAPTAIN, CAPTAIN},
    {CAPTAIN, CONTESSA},
    {CAPTAIN, DUKE},
    {CONTESSA, CONTESSA},
    {CONTESSA, DUKE},
    {DUKE, DUKE}
}};

const int NUM_ACTIONS = 27;
const int NUM_HOLDINGS = 15;
const int COIN_TO_ASSASSINATE = 3;
const int COIN_TO_COUP = 7;
const int COIN_TO_MUST_COUP = 10;

using ActionMask = uint32_t; // 32-bit bitmask where each bit corresponds to an Action enum value

class GameState {
public:
  int current_player;
  std::array<Card, 2> p1_cards;
  std::array<Card, 2> p2_cards;
  std::array<int, 2> p1_influence;
  std::array<int, 2> p2_influence;
  int p1_coins;
  int p2_coins;
  std::vector<Action> history;

  // MOST LIKELY #6
  int num_p1_has_allowed_tax;
  int num_p2_has_allowed_tax;
  int num_p1_has_allowed_block_fa;
  int num_p2_has_allowed_block_fa;

  // LIKELY #1
  int num_p1_has_allowed_foreign_aid;
  int num_p2_has_allowed_foreign_aid;
  int num_p1_has_allowed_steal;
  int num_p2_has_allowed_steal;
  int num_p1_has_allowed_assassinate;
  int num_p2_has_allowed_assassinate;

  // LIKELY #2
  int num_p1_has_claimed_duke;
  int num_p2_has_claimed_duke;
  int num_p1_has_claimed_steal_blocker;
  int num_p2_has_claimed_steal_blocker;
  int num_p1_has_claimed_contessa;
  int num_p2_has_claimed_contessa;

  // LIKELY #3 - Snapshot variables when each player loses first influence
  int p1_claims_duke_at_first_loss;
  int p1_claims_steal_blocker_at_first_loss;
  int p1_claims_contessa_at_first_loss;
  int p2_claims_duke_at_first_loss;
  int p2_claims_steal_blocker_at_first_loss;
  int p2_claims_contessa_at_first_loss;

public:
  GameState();
  void reset();
  bool is_terminal() const;
  double get_utility() const;
  double get_br_utility(int, std::array<double, NUM_HOLDINGS>) const;
  int get_current_player() const;
  void set_cards(Card, Card, Card, Card);
  void set_my_cards(const std::array<Card, 2>);
  std::vector<Action> get_legal_actions() const;
  std::vector<Action> get_card_losing_actions(const std::array<Card, 2>&, const std::array<int, 2>&) const;
  

  bool has_allowed_foreign_aid() const;
  bool has_allowed_steal() const;
  bool has_allowed_assassinate() const;

  bool has_opponent_allowed_tax() const;
  bool has_opponent_allowed_foreign_aid() const;
  bool has_opponent_allowed_steal() const;

  bool has_opponent_claimed_duke_2v2(bool is_p1) const;
  bool has_opponent_claimed_steal_blocker_2v2(bool is_p1) const;
  bool has_opponent_claimed_contessa_2v2(bool is_p1) const;

  bool has_opponent_claimed_duke_xv1(bool is_p1) const;
  bool has_opponent_claimed_steal_blocker_xv1(bool is_p1) const;
  bool has_opponent_claimed_contessa_xv1(bool is_p1) const;

  bool is_free_turn() const;

  // Mask helper: OR losing-card bits into legal_mask (no return)
  void set_card_losing_bits(const std::array<Card, 2>&, const std::array<int, 2>&, ActionMask&) const;
  
  // Rule enforcement helpers
  bool can_2v1_coupmate(int my_coins, int opp_coins, const std::array<Card, 2>& my_cards) const;
  
  void lose_card(Card);
  void undo_lose_card(Card);
  void do_action(Action);
  void undo_action();
  size_t get_history_hash() const;
  static size_t get_history_hash(const std::vector<Action>& history);
  size_t get_infoset_hash() const;
  std::string get_infoset_string() const;

  // For debugging
  void print_history() const;
  void print_game_state() const;
  std::string get_game_state() const; // for comparing two game states (e.g., testing do/undo)
};

#endif