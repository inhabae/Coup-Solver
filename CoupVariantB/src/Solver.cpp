#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

#include "CoupVariantBState.hpp"

template<typename T>
inline void hash_combine(std::size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline size_t get_information_set_hash(const CoupVariantBState& state) {
    size_t h = 0;
    hash_combine(h, state.get_current_player());
    hash_combine(h, state.get_current_player() == 0 ? state.player1_card : state.player2_card);
    for (auto a : state.action_history) hash_combine(h, static_cast<int>(a));
    return h;
}

inline size_t get_history_hash(const CoupVariantBState& state) {
    size_t h = 0;
    hash_combine(h, state.player1_card);
    hash_combine(h, state.player2_card);
    for (auto a : state.action_history) hash_combine(h, static_cast<int>(a));
    return h;
}

// Fixed-size InformationSet for CFR+
class InformationSet {
public:
    static constexpr int MAX_ACTIONS = 16;
    size_t key;
    Action legal[MAX_ACTIONS];
    int num_actions;
    double regret_sum[MAX_ACTIONS];
    double strat_sum[MAX_ACTIONS];
    double strategy[MAX_ACTIONS];
    double avg_strategy[MAX_ACTIONS];
    bool strat_dirty;
    bool avg_dirty;

    InformationSet(size_t id, const std::vector<Action>& actions)
        : key(id), num_actions(actions.size()), strat_dirty(true), avg_dirty(true) {
        for (int i = 0; i < num_actions; ++i) {
            legal[i] = actions[i];
            regret_sum[i] = strat_sum[i] = strategy[i] = avg_strategy[i] = 0.0;
        }
    }

    // Regret-matching strategy
    const double* get_strategy() {
        if (strat_dirty) {
            double sum_pos_regret = 0;
            for (int i = 0; i < num_actions; ++i)
                sum_pos_regret += std::max(regret_sum[i], 0.0);
            if (sum_pos_regret > 0) {
                for (int i = 0; i < num_actions; ++i)
                    strategy[i] = std::max(regret_sum[i], 0.0) / sum_pos_regret;
            } else {
                double u = 1.0 / num_actions;
                for (int i = 0; i < num_actions; ++i) strategy[i] = u;
            }
            strat_dirty = false;
        }
        return strategy;
    }

    // Average strategy
    const double* get_average_strategy() {
        if (avg_dirty) {
            double sum = std::accumulate(strat_sum, strat_sum + num_actions, 0.0);
            if (sum > 0) {
                for (int i = 0; i < num_actions; ++i)
                    avg_strategy[i] = strat_sum[i] / sum;
            } else {
                double u = 1.0 / num_actions;
                for (int i = 0; i < num_actions; ++i) avg_strategy[i] = u;
            }
            avg_dirty = false;
        }
        return avg_strategy;
    }

    void update_regret(int idx, double r) {
        regret_sum[idx] = std::max(regret_sum[idx] + r, 0.0);
        strat_dirty = true;
    }

    void update_strategy_sum(int idx, double v) {
        strat_sum[idx] += v;
        avg_dirty = true;
    }
};

class CFRPlusSolver {
private:
    std::unordered_map<size_t, std::unique_ptr<InformationSet>> infosets;
    std::unordered_map<size_t, std::string> hash2key;
    std::unordered_map<std::string, size_t> key2hash;
    std::unordered_map<size_t, std::vector<CoupVariantBState>> iset_states;
    std::unordered_map<size_t, double> opp_reach;
    std::unordered_map<size_t, Action> br_strategy;
    std::unordered_set<size_t> visited;
    int num_players = 2;
    int delay;

    InformationSet* get_iset(const CoupVariantBState& state) {
        size_t h = get_information_set_hash(state);
        auto it = infosets.find(h);
        if (it == infosets.end()) throw std::runtime_error("Missing info set");
        return it->second.get();
    }

    // Precompute all information sets by walking the tree
    void precompute(const CoupVariantBState& state) {
        if (state.is_terminal()) return;
        if (state.is_chance_node()) {
            std::vector<Card> C{ASSASSIN, CONTESSA, DUKE};
            for (auto c1 : C) for (auto c2 : C) {
                CoupVariantBState child = state;
                child.player1_card = c1;
                child.player2_card = c2;
                child.action_history.push_back(GAME_START);
                precompute(child);
            }
            return;
        }
        size_t h = get_information_set_hash(state);
        if (!infosets.count(h)) {
            auto acts = state.get_legal_actions();
            infosets[h] = std::make_unique<InformationSet>(h, acts);
            hash2key[h] = state.get_information_set_key();
            key2hash[state.get_information_set_key()] = h;
        }
        for (auto a : state.get_legal_actions()) {
            auto child_ptr = state.get_child(a);
            auto& child = *dynamic_cast<const CoupVariantBState*>(child_ptr.get());
            precompute(child);
        }
    }

public:
    CFRPlusSolver(int d) : delay(d) {
        infosets.reserve(4096);
    }

    void initialize(const CoupVariantBState& root) {
        std::cout << "Precomputing info sets...\n";
        auto t0 = std::chrono::high_resolution_clock::now();
        precompute(root);
        auto t1 = std::chrono::high_resolution_clock::now();
        std::cout << "Done. " << infosets.size() << " sets in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                  << "ms\n";
    }

    // Core CFR+ recursion
    std::vector<double> cfr_plus_recursive(
        const CoupVariantBState& state,
        Player trav,
        double weight,
        const std::vector<double>& reach) {
        if (state.is_terminal()) {
            auto U = state.get_utilities();
            std::vector<double> ret(U.size());
            for (size_t i = 0; i < U.size(); ++i)
                ret[i] = U[i] * reach[1 - trav];
            return ret;
        }
        if (state.is_chance_node()) {
            std::vector<double> EU(num_players, 0.0);
            std::vector<Card> C{ASSASSIN, CONTESSA, DUKE};
            for (auto c1 : C) for (auto c2 : C) {
                CoupVariantBState child = state;
                child.player1_card = c1;
                child.player2_card = c2;
                child.action_history.push_back(GAME_START);
                double init_prob = (c1 == c2) ? 2.0 / 30.0 : 4.0 / 30.0;
                auto cu = cfr_plus_recursive(child, trav, weight, reach);
                for (int p = 0; p < num_players; ++p) EU[p] += init_prob * cu[p];
            }
            return EU;
        }
        Player cur = state.get_current_player();
        auto iset = get_iset(state);
        const double* strat = iset->get_strategy();
        std::vector<double> node_util(num_players, 0.0);
        double action_util[InformationSet::MAX_ACTIONS][2];
        for (int i = 0; i < iset->num_actions; ++i) {
            Action a = iset->legal[i];
            auto child_ptr = state.get_child(a);
            auto& child = *dynamic_cast<const CoupVariantBState*>(child_ptr.get());
            auto new_reach = reach;
            if (cur != trav) new_reach[cur] *= strat[i];
            auto cu = cfr_plus_recursive(child, trav, weight, new_reach);
            action_util[i][0] = cu[0];
            action_util[i][1] = cu[1];
            for (int p = 0; p < num_players; ++p)
                node_util[p] += strat[i] * cu[p];
        }
        if (cur == trav) {
            for (int i = 0; i < iset->num_actions; ++i) {
                double r = action_util[i][trav] - node_util[trav];
                iset->update_regret(i, r);
            }
        } else {
            double opp = reach[cur];
            for (int i = 0; i < iset->num_actions; ++i)
                iset->update_strategy_sum(i, opp * strat[i] * (weight + 1));
        }
        return node_util;
    }

    // Calculate expected value following given strategies
    double calculate_expected_value(
        const CoupVariantBState& state,
        Player target,
        const std::unordered_map<std::string, std::unordered_map<Action,double>>& strat_map,
        double reach_prob) {
        if (state.is_terminal()) {
            auto U = state.get_utilities();
            return U[target];
        }
        if (state.is_chance_node()) {
            double util = 0;
            std::vector<Card> C{ASSASSIN, CONTESSA, DUKE};
            for (auto c1 : C) for (auto c2 : C) {
                CoupVariantBState child = state;
                child.player1_card = c1;
                child.player2_card = c2;
                child.action_history.push_back(GAME_START);
                double init_prob = (c1 == c2) ? 2.0 / 30.0 : 4.0 / 30.0;
                util += init_prob * calculate_expected_value(child, target, strat_map, reach_prob);
            }
            return util;
        }
        auto actions = state.get_legal_actions();
        auto key = state.get_information_set_key();
        std::unordered_map<Action,double> σ;
        auto it = strat_map.find(key);
        if (it != strat_map.end()) σ = it->second;
        else {
            double u = 1.0 / actions.size();
            for (auto a : actions) σ[a] = u;
        }
        double ev = 0;
        for (auto a : actions) {
            auto child_ptr = state.get_child(a);
            auto& child = *dynamic_cast<const CoupVariantBState*>(child_ptr.get());
            ev += σ[a] * calculate_expected_value(child, target, strat_map, reach_prob);
        }
        return ev;
    }

    // Compute exploitability
    double calculate_exploitability(const CoupVariantBState& root) {
        auto strat_map = get_all_strategies();
        // build hashed opponent strategies
        std::unordered_map<size_t, std::unordered_map<Action,double>> opp_strat_h;
        for (auto& kv : strat_map) {
            auto it = key2hash.find(kv.first);
            if (it == key2hash.end()) continue;
            opp_strat_h[it->second] = kv.second;
        }
        double total = 0;
        for (int player = 0; player < num_players; ++player) {
            // clear best-response data
            br_strategy.clear(); visited.clear(); iset_states.clear(); opp_reach.clear();
            // build tree for best response
            build_infosets_and_reach(root, player, opp_strat_h, 1.0);
            double best_val = calculate_best_response(root, player, opp_strat_h, 1.0);
            std::cout << "Player" << player + 1 << std::endl;
            std::cout << "\tBR EV: " << best_val << std::endl;
            double exp_val = calculate_expected_value(root, player, strat_map, 1.0);
            std::cout << "\tCurrent EV: " << exp_val << std::endl;
            if (best_val < exp_val) {
                std::cerr << "[ERROR] Best response strategy EV is lower than current strategy EV" << std::endl;
                std::exit(1);
            }
            total += best_val - exp_val;
        }
        return total;
    }

    // Build infosets and reach probabilities for best response
    void build_infosets_and_reach(
        const CoupVariantBState& state,
        Player br,
        const std::unordered_map<size_t, std::unordered_map<Action,double>>& opp_strat,
        double r) {
        size_t I = get_information_set_hash(state);
        iset_states[I].push_back(state);
        size_t H = get_history_hash(state);
        opp_reach[H] = r;
        if (state.is_terminal()) return;
        if (state.is_chance_node()) {
            std::vector<Card> C{ASSASSIN, CONTESSA, DUKE};
            for (auto c1 : C) for (auto c2 : C) {
                CoupVariantBState ch = state;
                ch.player1_card = c1;
                ch.player2_card = c2;
                ch.action_history.push_back(GAME_START);
                build_infosets_and_reach(ch, br, opp_strat, r);
            }
            return;
        }
        Player cur = state.get_current_player();
        auto actions = state.get_legal_actions();
        if (cur == br) {
            for (auto a : actions) {
                auto child_ptr = state.get_child(a);
                auto& ch = *dynamic_cast<const CoupVariantBState*>(child_ptr.get());
                build_infosets_and_reach(ch, br, opp_strat, r);
            }
        } else {
            auto it = opp_strat.find(I);
            for (auto a : actions) {
                double p = (it != opp_strat.end() ? it->second.at(a) : 1.0/actions.size());
                auto child_ptr = state.get_child(a);
                auto& ch = *dynamic_cast<const CoupVariantBState*>(child_ptr.get());
                build_infosets_and_reach(ch, br, opp_strat, r * p);
            }
        }
    }

    // Recursive best response (fixed)
    double calculate_best_response(
        const CoupVariantBState& state,
        Player br,
        const std::unordered_map<size_t, std::unordered_map<Action,double>>& opp_strat,
        double opp_reach_val) {
        // Terminal node
        if (state.is_terminal()) {
            return state.get_utilities()[br];
        }
        // Chance node
        if (state.is_chance_node()) {
            double exp_u = 0.0;
            std::vector<Card> cards = {ASSASSIN, CONTESSA, DUKE};
            for (auto c1 : cards) for (auto c2 : cards) {
                    CoupVariantBState child = state;
                    child.player1_card = c1;
                    child.player2_card = c2;
                    child.action_history.push_back(GAME_START);
                    double prob = (c1 == c2) ? 2.0 / 30.0 : 4.0 / 30.0;
                    exp_u += prob * calculate_best_response(child, br, opp_strat, opp_reach_val);
                }
            return exp_u;
        }
        // Decision node
        Player cur = state.get_current_player();
        size_t I = get_information_set_hash(state);
        auto actions = state.get_legal_actions();
        // Our node: pick one action per infoset
        if (cur == br) {
            if (!visited.count(I)) {
                std::unordered_map<Action,double> EV; 
                for (auto a : actions) EV[a] = 0.0;
                // aggregate over every history in the infoset
                for (const auto &hist_state : iset_states[I]) {
                    double reach = opp_reach[get_history_hash(hist_state)];
                    for (auto a : actions) {
                        auto child_ptr = hist_state.get_child(a);
                        auto &child = *dynamic_cast<const CoupVariantBState *>(child_ptr.get());
                        EV[a] += reach * calculate_best_response(child, br, opp_strat, reach);
                    }
                }
                // choose the action with max EV
                auto best = std::max_element(
                    EV.begin(), EV.end(),
                    [](auto &x, auto &y) { return x.second < y.second; }
                );
                br_strategy[I] = best->first;
                visited.insert(I);
            }
            // now always play the chosen action
            Action a = br_strategy[I];
            auto child_ptr = state.get_child(a);
            auto &child = *dynamic_cast<const CoupVariantBState *>(child_ptr.get());
            return calculate_best_response(child, br, opp_strat, opp_reach_val);
        }
        // Opponent node: fold in their σ
        double exp_u = 0.0;
        std::unordered_map<Action,double> σ;
        auto it = opp_strat.find(I);
        if (it != opp_strat.end()) σ = it->second;
        else {
            double uniform = 1.0 / actions.size();
            for (auto a : actions) σ[a] = uniform;
        }
        for (auto a : actions) {
            double p = σ[a];
            auto child_ptr = state.get_child(a);
            auto &child = *dynamic_cast<const CoupVariantBState *>(child_ptr.get());
            exp_u += p * calculate_best_response(child, br, opp_strat, opp_reach_val * p);
        }
        return exp_u;
    }

    // Train with exploitability tracking
    void train_with_exploitability_tracking(
        const CoupVariantBState& root,
        int max_iter,
        int check_interval,
        double target_exploit) {
        std::cout << "Starting training...\n";
        for (int t = 1; t <= max_iter; ++t) {
            double w = std::max(t - delay, 0);
            for (int p = 0; p < num_players; ++p) {
                std::vector<double> reach(num_players, 1.0);
                cfr_plus_recursive(root, p, w, reach);
            }
            if (t % check_interval == 0) {
                double e = calculate_exploitability(root);
                std::cout << "Iteration " << t << ": exploitability = " << e << "\n";
                if (e <= target_exploit) break;
            }
        }
    }

    // Extract all average strategies
    std::unordered_map<std::string, std::unordered_map<Action,double>> get_all_strategies() {
        std::unordered_map<std::string, std::unordered_map<Action,double>> strat;
        for (auto& kv : infosets) {
            size_t h = kv.first;
            auto* iset = kv.second.get();
            const double* avg = iset->get_average_strategy();
            auto& m = strat[hash2key[h]];
            for (int i = 0; i < iset->num_actions; ++i)
                m[iset->legal[i]] = avg[i];
        }
        return strat;
    }

    // Binary save/load
    void save_strategies_binary(const std::string& filename) {
        auto strat = get_all_strategies();
        std::ofstream file(filename, std::ios::binary);
        size_t n = strat.size();
        file.write(reinterpret_cast<const char*>(&n), sizeof(n));
        for (auto& kv : strat) {
            const auto& key = kv.first;
            size_t len = key.size();
            file.write(reinterpret_cast<const char*>(&len), sizeof(len));
            file.write(key.data(), len);
            auto& m = kv.second;
            size_t na = m.size();
            file.write(reinterpret_cast<const char*>(&na), sizeof(na));
            for (auto& ap : m) {
                file.write(reinterpret_cast<const char*>(&ap.first), sizeof(ap.first));
                file.write(reinterpret_cast<const char*>(&ap.second), sizeof(ap.second));
            }
        }
    }

    std::unordered_map<std::string, std::unordered_map<Action,double>> load_strategies_binary(const std::string& filename) {
        std::unordered_map<std::string, std::unordered_map<Action,double>> strat;
        std::ifstream file(filename, std::ios::binary);
        size_t n; file.read(reinterpret_cast<char*>(&n), sizeof(n));
        while (n--) {
            size_t len; file.read(reinterpret_cast<char*>(&len), sizeof(len));
            std::string key(len, '\0'); file.read(&key[0], len);
            size_t na; file.read(reinterpret_cast<char*>(&na), sizeof(na));
            for (size_t i = 0; i < na; ++i) {
                Action a; double p;
                file.read(reinterpret_cast<char*>(&a), sizeof(a));
                file.read(reinterpret_cast<char*>(&p), sizeof(p));
                strat[key][a] = p;
            }
        }
        return strat;
    }
};

int main() {
    CoupVariantBState root;
    CFRPlusSolver solver(100);
    solver.initialize(root);
    solver.train_with_exploitability_tracking(root, 1000000, 100, 0.01);
    auto strat = solver.get_all_strategies();
    // e.g., save for later
    solver.save_strategies_binary("strategies.bin");
    std::cout << "Training complete.\n";
    return 0;
}
