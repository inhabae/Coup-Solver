#include "trainer.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <sstream>

Trainer::Trainer() : current_utility(0.0) {}

std::vector<double> Trainer::get_strategy(size_t infoset) {
    const std::vector<double>& regrets = regret_sum[infoset];
    std::vector<double> s(regrets.size(), 0.0);
    double total = 0.0;
    for (double r : regrets) if (r > 0) total += r;
    if (total > 0)
        for (size_t i = 0; i < regrets.size(); ++i)
            s[i] = std::max(0.0, regrets[i]) / total;
    else
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = 1.0 / s.size();
    return s;
}

std::vector<double> Trainer::get_average_strategy(size_t infoset) {
    std::vector<double> avg = strategy_sum[infoset];
    double sum = 0.0;
    for (double p : avg) sum += p;
    if (sum > 0)
        for (double& p : avg) p /= sum;
    else
        for (double& p : avg) p = 1.0 / avg.size();
    return avg;
}

double Trainer::cfr(GameState& g, double p1_reach, double p2_reach) {
    if (g.history.size() > 40) {
        std::cout << "History depth exceeded: " << g.history.size() << '\n';
        g.print_game_state();
        return 0.0;
    }

    if (g.is_terminal()) return g.get_utility();

    const int player = g.get_current_player();
    std::vector<Action> legal_actions = g.get_legal_actions();
    std::vector<Action> actions;

    // 2v2 pruning: only consider actions leading into the precomputed valid tree.
    const int p1_lives = g.p1_influence[0] + g.p1_influence[1];
    const int p2_lives = g.p2_influence[0] + g.p2_influence[1];
    if (p1_lives == 2 && p2_lives == 2) {
        for (Action a : legal_actions) {
            g.do_action(a);
            size_t hh = g.get_history_hash();
            g.undo_action();
            if (valid_histories.count(hh)) actions.push_back(a);
        }
        // After a challenge / card-loss the valid_histories filter may not apply;
        // fall back to full legal set in that case.
        if (actions.empty()) actions = legal_actions;
    } else {
        actions = legal_actions;
    }

    const size_t infoset = g.get_infoset_hash();
    const std::string infoset_str = g.get_infoset_string();

    // Collision detection.
    if (hash_to_string.count(infoset) && hash_to_string[infoset] != infoset_str)
        assert(false && "Infoset hash collision");

    // Lazy init.
    if (!regret_sum.count(infoset)) {
        regret_sum[infoset]    = std::vector<double>(actions.size(), 0.0);
        strategy_sum[infoset]  = std::vector<double>(actions.size(), 0.0);
        next_actions[infoset]  = actions;
        hash_to_string[infoset] = infoset_str;
    }

    std::vector<double> strategy    = get_strategy(infoset);
    std::vector<double> action_util(actions.size(), 0.0);
    double node_util = 0.0;

    for (size_t i = 0; i < actions.size(); ++i) {
        g.do_action(actions[i]);
        double np1 = (player == 0) ? p1_reach * strategy[i] : p1_reach;
        double np2 = (player == 1) ? p2_reach * strategy[i] : p2_reach;
        action_util[i] = -cfr(g, np1, np2);
        g.undo_action();
        node_util += strategy[i] * action_util[i];
    }

    const double reach = (player == 0) ? p2_reach : p1_reach;
    for (size_t i = 0; i < actions.size(); ++i) {
        regret_sum[infoset][i]   += reach * (action_util[i] - node_util);
        strategy_sum[infoset][i] += ((player == 0) ? p1_reach : p2_reach) * strategy[i];
    }

    return node_util;
}

double Trainer::calculate_best_response(GameState& g, const int max_player,
                                         std::array<double, NUM_HOLDINGS> pair_dist) {
    if (g.is_terminal()) return g.get_br_utility(max_player, pair_dist);

    if (g.get_current_player() == max_player) {
        std::vector<Action> actions = g.get_legal_actions();
        double best = -100.0;
        for (Action a : actions) {
            g.do_action(a);
            double v = -calculate_best_response(g, max_player, pair_dist);
            g.undo_action();
            best = std::max(best, v);
        }
        return best;
    } else {
        std::array<double, NUM_ACTIONS> action_probs = {};
        std::array<std::array<double, NUM_HOLDINGS>, NUM_ACTIONS> new_dist = {};

        for (size_t h = 0; h < NUM_HOLDINGS; ++h) {
            g.set_my_cards(holdings[h]);
            std::vector<Action> legal = g.get_legal_actions();
            std::vector<double> avg   = get_average_strategy(g.get_infoset_hash());
            for (size_t i = 0; i < legal.size(); ++i) {
                int ai = static_cast<int>(legal[i]);
                double v = avg[i] * pair_dist[h];
                action_probs[ai]    += v;
                new_dist[ai][h]      = v;
            }
        }

        double node_util = 0.0;
        for (size_t ai = 0; ai < NUM_ACTIONS; ++ai) {
            if (action_probs[ai] == 0.0) continue;
            std::array<double, NUM_HOLDINGS> nd = new_dist[ai];
            for (double& p : nd) p /= action_probs[ai];
            g.do_action(static_cast<Action>(ai));
            node_util += action_probs[ai] * -calculate_best_response(g, max_player, nd);
            g.undo_action();
        }
        return node_util;
    }
}

void Trainer::train(size_t iterations) {
    std::vector<Card> pool = {
        ASSASSIN,ASSASSIN,ASSASSIN,
        AMBASSADOR,AMBASSADOR,AMBASSADOR,
        CAPTAIN,CAPTAIN,CAPTAIN,
        CONTESSA,CONTESSA,CONTESSA,
        DUKE,DUKE,DUKE
    };
    std::random_device rd;
    std::mt19937 gen(rd());
    double util = 0.0;

    for (size_t i = 0; i < iterations; ++i) {
        std::shuffle(pool.begin(), pool.end(), gen);
        GameState g(RulesConfig::solver_default());
        g.set_cards(pool[0], pool[1], pool[2], pool[3]);
        util += cfr(g, 1.0, 1.0);
    }

    current_utility = util / iterations;
    std::cout << "util: " << util << ", iterations: " << iterations << '\n';
    std::cout << "Average game value: " << current_utility << '\n';
}

int Trainer::card_to_index(Card c) {
    switch (c) {
        case ASSASSIN:  return 0;
        case AMBASSADOR:return 1;
        case CAPTAIN:   return 2;
        case CONTESSA:  return 3;
        case DUKE:      return 4;
        default:        return -1;
    }
}

std::array<double, NUM_HOLDINGS> Trainer::calculate_pair_distribution(Card r1, Card r2) {
    std::vector<int> cnt = {3,3,3,3,3};
    cnt[card_to_index(r1)]--;
    cnt[card_to_index(r2)]--;
    constexpr double INV = 1.0 / 78.0;
    std::array<double, NUM_HOLDINGS> dist{};
    for (size_t h = 0; h < NUM_HOLDINGS; ++h) {
        int i = card_to_index(holdings[h][0]);
        int j = card_to_index(holdings[h][1]);
        dist[h] = (i == j)
            ? (cnt[i] >= 2 ? cnt[i]*(cnt[i]-1)*0.5*INV : 0.0)
            : (cnt[i] >= 1 && cnt[j] >= 1 ? cnt[i]*cnt[j]*INV : 0.0);
    }
    return dist;
}

double Trainer::calculate_pair_probability(Card c1, Card c2) {
    return (c1 == c2) ? 3.0/105.0 : 9.0/105.0;
}

void Trainer::calculate_exploitability() {
    double p1_br = 0.0, p2_br = 0.0;
    for (size_t c = 0; c < holdings.size(); ++c) {
        Card c1 = holdings[c][0], c2 = holdings[c][1];
        auto dist = calculate_pair_distribution(c1, c2);
        for (int p = 0; p < 2; ++p) {
            GameState g(RulesConfig::solver_default());
            g.set_cards(c1, c2, ASSASSIN, ASSASSIN);
            double v = calculate_best_response(g, p, dist) * calculate_pair_probability(c1, c2);
            if (p == 0) p1_br += v; else p2_br -= v;
        }
    }
    std::cout << "P1 BR EV: " << p1_br << '\n';
    std::cout << "P2 BR EV: " << p2_br << '\n';
    std::cout << "P1 exploitability: " << p1_br - current_utility << '\n';
    std::cout << "P2 exploitability: " << p2_br + current_utility << '\n';
}

// ── Debugging utilities (unchanged logic, updated for new GameState) ──

void Trainer::get_tree_size(size_t max_depth) {
    GameState g(RulesConfig::solver_default());
    g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    std::function<size_t(GameState&, size_t)> count =
        [&](GameState& s, size_t d) -> size_t {
            size_t n = 1;
            if (d >= max_depth || s.is_terminal()) return n;
            for (Action a : s.get_legal_actions()) {
                s.do_action(a); n += count(s, d+1); s.undo_action();
            }
            return n;
        };
    std::cout << "MAX_DEPTH " << max_depth << " histories: " << count(g, 0) << '\n';
}

void Trainer::get_2v2_tree_size(size_t max_depth) {
    GameState g(RulesConfig::solver_default());
    g.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    std::function<size_t(GameState&, size_t)> count =
        [&](GameState& s, size_t d) -> size_t {
            size_t n = 1;
            if (g.p1_influence[0]+g.p1_influence[1] < 2 ||
                g.p2_influence[0]+g.p2_influence[1] < 2 ||
                d >= max_depth) return n;
            for (Action a : s.get_legal_actions()) {
                s.do_action(a); n += count(s, d+1); s.undo_action();
            }
            return n;
        };
    std::cout << "2v2 MAX_DEPTH " << max_depth << " histories: " << count(g, 0) << '\n';
}

void Trainer::collect_histories(GameState& g, std::set<std::string>& histories,
                                 size_t max_depth, bool /*use_original*/) {
    if (g.history.size() >= max_depth || g.is_terminal()) {
        histories.insert(history_to_string(g.history));
        return;
    }
    histories.insert(history_to_string(g.history));
    for (Action a : g.get_legal_actions()) {
        g.do_action(a);
        collect_histories(g, histories, max_depth, false);
        g.undo_action();
    }
}

std::string Trainer::history_to_string(const std::vector<Action>& h) {
    std::string s;
    for (Action a : h) { s += std::to_string(a); s += ','; }
    return s;
}

void Trainer::find_2v2_terminals(GameState& g, size_t move_limit) {
    if (g.history.size() > move_limit) return;

    if (!g.history.empty()) {
        Action last = g.history.back();
        bool is_terminal_event =
            (last == CHALLENGE) ||
            (last >= LOSE_ASSASSIN && last <= LOSE_BOTH);
        if (is_terminal_event) {
            size_t hh = g.get_history_hash();
            auto it = history_map.find(hh);
            if (it != history_map.end()) {
                assert(it->second == g.history && "Hash collision in find_2v2_terminals");
            } else {
                history_map[hh] = g.history;
            }
            valid_histories.insert(hh);
            return;
        }
    }

    for (Action a : g.get_legal_actions()) {
        g.do_action(a);
        find_2v2_terminals(g, move_limit);
        g.undo_action();
    }
}

std::unordered_set<size_t> Trainer::find_all_2v2_terminals(size_t move_limit) {
    std::unordered_set<size_t> terminals;

    for (const auto& holding : holdings) {
        GameState g(RulesConfig::solver_default());
        g.set_cards(holding[0], holding[1], holding[0], holding[1]);
        find_2v2_terminals(g, move_limit);
    }

    terminals = valid_histories;
    valid_histories.clear();

    for (size_t th : terminals) {
        const std::vector<Action>& hist = history_map[th];
        std::vector<Action> prefix;
        prefix.reserve(hist.size());
        for (Action a : hist) {
            prefix.push_back(a);
            size_t ph = GameState::get_history_hash(prefix);
            auto it = history_map.find(ph);
            if (it != history_map.end()) {
                assert(it->second == prefix && "Hash collision in prefix generation");
            } else {
                history_map[ph] = prefix;
            }
            valid_histories.insert(ph);
        }
    }

    valid_histories.insert(terminals.begin(), terminals.end());
    return valid_histories;
}

void Trainer::compare_tree_size(size_t max_size) {
    GameState g1(RulesConfig::solver_default());
    g1.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    std::set<std::string> new_h, orig_h;
    collect_histories(g1, new_h, max_size, false);

    GameState g2(RulesConfig::solver_default());
    g2.set_cards(ASSASSIN, ASSASSIN, ASSASSIN, AMBASSADOR);
    collect_histories(g2, orig_h, max_size, true);

    std::cout << "New: " << new_h.size() << "  Orig: " << orig_h.size() << '\n';
}