#include "small_coup_rebel.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace small_coup;
using namespace small_coup::rebel;

namespace {

int parse_int_arg(const char* value, const char* name) {
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (*end != '\0' || parsed < 0) throw std::invalid_argument(std::string("invalid ") + name);
    return static_cast<int>(parsed);
}

const char* action_color(Action action) {
    switch (action) {
        case Action::Income: return "#16a34a";
        case Action::Assassinate: return "#f97316";
        case Action::Coup: return "#7c3aed";
        case Action::Allow: return "#6b7280";
        case Action::BlockAssassinate: return "#2563eb";
        case Action::Challenge: return "#dc2626";
        case Action::LoseLife: return "#374151";
        case Action::Count: return "#111827";
    }
    return "#111827";
}

std::string history_json(const PublicState& public_state) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < public_state.history.size(); ++i) {
        if (i > 0) out << ",";
        out << "{\"player\":" << public_state.history[i].player
            << ",\"action\":\"" << action_name(public_state.history[i].action) << "\"}";
    }
    out << "]";
    return out.str();
}

std::string actions_json(ActionMask legal_mask, const SearchResult& result) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (Action action : actions_from_mask(legal_mask)) {
        if (!first) out << ",";
        first = false;
        const int index = static_cast<int>(action);
        out << "{\"name\":\"" << action_name(action) << "\","
            << "\"frequency\":" << std::fixed << std::setprecision(6)
            << result.policy[static_cast<std::size_t>(index)] << ","
            << "\"color\":\"" << action_color(action) << "\"}";
    }
    out << "]";
    return out.str();
}

std::string spot_label(const PublicState& public_state) {
    std::ostringstream out;
    out << "P" << public_state.current_player << " " << phase_name(public_state.phase)
        << " | coins " << public_state.coins[0] << "-" << public_state.coins[1]
        << " | rebel";
    return out.str();
}

void write_rebel_export(const std::string& path, int depth, int resolve_iterations, uint32_t seed) {
    HeuristicValueEvaluator evaluator;
    DepthLimitedResolver resolver(resolve_iterations, depth, evaluator, seed);
    std::map<std::string, GameState> states;
    std::queue<std::pair<GameState, int>> frontier;
    for (const Deal& deal : all_deals()) {
        GameState state(deal);
        const std::string key = public_state_from(state).serialize();
        if (states.emplace(key, state).second) frontier.push({state, 0});
    }
    while (!frontier.empty()) {
        const auto [state, state_depth] = frontier.front();
        frontier.pop();
        if (state_depth >= depth || state.is_terminal()) continue;
        for (Action action : actions_from_mask(state.legal_actions())) {
            GameState child = state;
            child.apply(action);
            const std::string key = public_state_from(child).serialize();
            if (states.emplace(key, child).second) frontier.push({child, state_depth + 1});
        }
    }

    std::ofstream out(path);
    if (!out) throw std::runtime_error("could not open JSON output path");
    out << "{\n";
    out << "  \"stats\": {\"solver\": \"rebel\", \"iterations\": " << resolve_iterations
        << ", \"infosets\": " << states.size()
        << ", \"avgUtilityP0\": 0},\n";
    out << "  \"actions\": [";
    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        if (i > 0) out << ",";
        const Action action = static_cast<Action>(i);
        out << "{\"name\":\"" << action_name(action) << "\",\"color\":\"" << action_color(action) << "\"}";
    }
    out << "],\n";
    out << "  \"spots\": [\n";
    std::size_t index = 0;
    for (const auto& item : states) {
        const GameState& state = item.second;
        const PublicState public_state = public_state_from(state);
        const SearchResult result = state.is_terminal()
            ? SearchResult{}
            : resolver.resolve(state, state.current_player());
        out << "    {\"id\":\"rebel-spot-" << index << "\","
            << "\"label\":\"" << spot_label(public_state) << "\","
            << "\"player\":" << public_state.current_player << ","
            << "\"phase\":\"" << phase_name(public_state.phase) << "\","
            << "\"currentPlayer\":" << public_state.current_player << ","
            << "\"coins\":[" << public_state.coins[0] << "," << public_state.coins[1] << "],"
            << "\"lives\":[" << public_state.lives[0] << "," << public_state.lives[1] << "],"
            << "\"assassinated\":[" << (public_state.assassinated[0] ? "true" : "false")
            << "," << (public_state.assassinated[1] ? "true" : "false") << "],"
            << "\"history\":" << history_json(public_state) << ","
            << "\"visits\":1,"
            << "\"availableActions\":" << actions_json(public_state.legal_mask, result) << ","
            << "\"holdings\":[]}";
        if (++index < states.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void print_usage(const char* name) {
    std::cerr << "usage:\n"
              << "  " << name << " benchmark [iterations] [seed] [max_public_actions]\n"
              << "  " << name << " export-json <output.json> [seed] [resolve_iters] [depth]\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 2;
        }
        const std::string command = argv[1];
        if (command == "benchmark") {
            const int iterations = argc > 2 ? parse_int_arg(argv[2], "iterations") : 10000;
            const uint32_t seed = argc > 3 ? static_cast<uint32_t>(parse_int_arg(argv[3], "seed")) : 1;
            const int max_public_actions = argc > 4 ? parse_int_arg(argv[4], "max public actions") : 16;
            const auto started = std::chrono::steady_clock::now();
            CfrTrainer trainer(seed, max_public_actions);
            const TrainingStats stats = trainer.train(iterations);
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cout << "mode=small_coup_cfr"
                      << " iterations=" << stats.iterations
                      << " seed=" << seed
                      << " max_public_actions=" << max_public_actions
                      << " infosets=" << stats.infosets
                      << " avg_utility_p0=" << std::fixed << std::setprecision(6)
                      << (stats.iterations > 0 ? stats.utility0_sum / stats.iterations : 0.0)
                      << " seconds=" << elapsed
                      << "\n";
            return 0;
        }
        if (command == "export-json") {
            if (argc < 3) {
                print_usage(argv[0]);
                return 2;
            }
            const std::string path = argv[2];
            const uint32_t seed = argc > 3 ? static_cast<uint32_t>(parse_int_arg(argv[3], "seed")) : 1;
            const int resolve_iters = argc > 4 ? parse_int_arg(argv[4], "resolve iterations") : 64;
            const int depth = argc > 5 ? parse_int_arg(argv[5], "depth") : 4;
            write_rebel_export(path, depth, resolve_iters, seed);
            std::cout << "wrote rebel_export path=" << path
                      << " seed=" << seed
                      << " resolve_iters=" << resolve_iters
                      << " depth=" << depth
                      << "\n";
            return 0;
        }
        print_usage(argv[0]);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "small_coup_rebel error: " << error.what() << "\n";
        return 1;
    }
}
