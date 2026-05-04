#include "trainer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace coup;

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_stop_signal(int) {
    stop_requested = 1;
}

int parse_int_arg(const char* value, const char* name) {
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (*end != '\0' || parsed < 0) {
        throw std::invalid_argument(std::string("invalid ") + name);
    }
    return static_cast<int>(parsed);
}

bool is_infinite_arg(const std::string& value) {
    return value == "inf" || value == "infinite" || value == "forever";
}

double strategy_weight(const InfosetNode& node) {
    double total = 0.0;
    for (CfrValue value : node.strategy_sum) total += value;
    return total;
}

// ── Label regeneration ────────────────────────────────────────────────────────
//
// debug_label is no longer stored in InfosetNode. We rebuild it on demand
// (only at export/print time) by walking the shared ObservationStore.

static const char* obs_kind_name(ObservationTokenKind kind) {
    switch (kind) {
        case ObservationTokenKind::InitialCards:           return "InitialCards";
        case ObservationTokenKind::PublicAction:           return "PublicAction";
        case ObservationTokenKind::PublicReveal:           return "PublicReveal";
        case ObservationTokenKind::PublicLose:             return "PublicLose";
        case ObservationTokenKind::PublicReplacement:      return "PublicReplacement";
        case ObservationTokenKind::PublicExchangeComplete: return "PublicExchangeComplete";
        case ObservationTokenKind::PrivateReplacement:     return "PrivateReplacement";
        case ObservationTokenKind::PrivateExchangeDraw:    return "PrivateExchangeDraw";
        case ObservationTokenKind::PrivateExchangeKeep:    return "PrivateExchangeKeep";
    }
    return "Unknown";
}

static void append_obs_chain(std::ostringstream& out, const char* label,
                              uint32_t id, const ObservationStore& store) {
    std::vector<ObservationToken> chain;
    uint32_t cur = id;
    while (cur != 0) {
        const ObservationNode& nd = store.node(cur);
        chain.push_back(nd.token);
        cur = nd.parent;
    }
    std::reverse(chain.begin(), chain.end());
    out << "\n" << label << "=";
    for (const ObservationToken& tok : chain) {
        out << " [" << obs_kind_name(tok.kind) << " p=" << static_cast<int>(tok.player);
        if (tok.kind == ObservationTokenKind::PublicAction  ||
            tok.kind == ObservationTokenKind::PublicReveal  ||
            tok.kind == ObservationTokenKind::PublicLose    ||
            tok.kind == ObservationTokenKind::PrivateExchangeKeep) {
            out << " a=" << action_name(static_cast<Action>(tok.action));
        }
        if (static_cast<Card>(tok.card0) != Card::None) out << " c0=" << card_name(static_cast<Card>(tok.card0));
        if (static_cast<Card>(tok.card1) != Card::None) out << " c1=" << card_name(static_cast<Card>(tok.card1));
        if (static_cast<Card>(tok.card2) != Card::None) out << " c2=" << card_name(static_cast<Card>(tok.card2));
        if (static_cast<Card>(tok.card3) != Card::None) out << " c3=" << card_name(static_cast<Card>(tok.card3));
        out << "]";
    }
}

// Walk the private obs chain to find which player owns this node.
static int player_from_key(const InfosetKey& key, const ObservationStore& store) {
    uint32_t cur = key.private_obs_id;
    while (cur != 0) {
        const ObservationNode& nd = store.node(cur);
        if (nd.token.kind == ObservationTokenKind::InitialCards)
            return static_cast<int>(nd.token.player);
        cur = nd.parent;
    }
    return 0;
}

// Regenerates the debug label string from key + store.
// Format mirrors GameState::infoset_debug_string so parsing helpers still work.
static std::string regenerate_debug_label(const InfosetKey& key,
                                          const ObservationStore& store) {
    const int player = player_from_key(key, store);
    std::ostringstream out;
    out << "key public=" << key.public_obs_id
        << " private=" << key.private_obs_id
        << " player=" << player;
    append_obs_chain(out, "public",  key.public_obs_id,  store);
    append_obs_chain(out, "private", key.private_obs_id, store);
    return out.str();
}

// ── Memory polling ────────────────────────────────────────────────────────────

struct MemStats {
    long vm_rss_kb{-1};
    long vm_peak_kb{-1};
    long vm_size_kb{-1};
};

MemStats read_mem_stats() {
    MemStats out;
    std::ifstream f("/proc/self/status");
    if (!f) return out;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0)  out.vm_rss_kb  = std::stol(line.substr(6));
        if (line.rfind("VmPeak:", 0) == 0) out.vm_peak_kb = std::stol(line.substr(7));
        if (line.rfind("VmSize:", 0) == 0) out.vm_size_kb = std::stol(line.substr(7));
    }
    return out;
}

// With fixed arrays there is no heap allocation per node beyond the map itself.
struct NodeSizeBreakdown {
    std::size_t node_count{0};
    std::size_t struct_bytes{0};   // sizeof(InfosetNode) * count (arrays are inline)
    std::size_t bucket_bytes{0};   // unordered_map bucket array overhead
};

NodeSizeBreakdown compute_node_breakdown(const CfrTrainer& trainer) {
    const auto& nodes = trainer.nodes();
    NodeSizeBreakdown b;
    b.node_count   = nodes.size();
    b.struct_bytes = nodes.size() * sizeof(InfosetNode);
    b.bucket_bytes = nodes.bucket_count() * sizeof(void*);
    return b;
}

void log_mem(std::ostream& out, const CfrTrainer& trainer) {
    const MemStats mem = read_mem_stats();
    const NodeSizeBreakdown b = compute_node_breakdown(trainer);
    out << "[mem]"
        << " rss_MB="       << (mem.vm_rss_kb  >= 0 ? mem.vm_rss_kb  / 1024 : -1)
        << " peak_MB="      << (mem.vm_peak_kb >= 0 ? mem.vm_peak_kb / 1024 : -1)
        << " virt_MB="      << (mem.vm_size_kb >= 0 ? mem.vm_size_kb / 1024 : -1)
        << " | nodes="      << b.node_count
        << " struct_MB="    << b.struct_bytes / 1'000'000
        << " bucket_MB="    << b.bucket_bytes / 1'000'000
        << " est_total_MB=" << (b.struct_bytes + b.bucket_bytes) / 1'000'000
        << " sizeof_node="  << sizeof(InfosetNode)
        << "\n";
    out.flush();
}

// ── Parsing helpers (unchanged logic) ────────────────────────────────────────

std::string extract_between(const std::string& value, const std::string& start_marker,
                            const std::string& end_marker) {
    const std::size_t start_pos = value.find(start_marker);
    if (start_pos == std::string::npos) return "";
    const std::size_t start = start_pos + start_marker.size();
    const std::size_t end = end_marker.empty() ? std::string::npos : value.find(end_marker, start);
    return value.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

int extract_int_after(const std::string& value, const std::string& marker) {
    const std::size_t marker_pos = value.find(marker);
    if (marker_pos == std::string::npos) return 0;
    const std::size_t start = marker_pos + marker.size();
    return std::stoi(value.substr(start));
}

std::string extract_word_after(const std::string& value, const std::string& marker) {
    const std::size_t marker_pos = value.find(marker);
    if (marker_pos == std::string::npos) return "";
    const std::size_t start = marker_pos + marker.size();
    const std::size_t end = value.find_first_of(" \n", start);
    return value.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

std::string private_label(const std::string& debug_label) {
    const std::string chain = extract_between(debug_label, "\nprivate=", "");
    if (chain.empty()) return "private";
    std::string label;
    std::size_t pos = 0;
    while ((pos = chain.find("[", pos)) != std::string::npos) {
        const std::size_t end = chain.find("]", pos);
        if (end == std::string::npos) break;
        const std::string token = chain.substr(pos + 1, end - pos - 1);
        if (token.rfind("InitialCards", 0) == 0) {
            std::string card0 = extract_word_after(token, "c0=");
            std::string card1 = extract_word_after(token, "c1=");
            if (card1 < card0) std::swap(card0, card1);
            label = card0 + "/" + card1;
        } else if (token.rfind("PrivateReplacement", 0) == 0) {
            label += " -> " + extract_word_after(token, "c0=");
        } else if (token.rfind("PrivateExchangeDraw", 0) == 0) {
            label += " draw " + extract_word_after(token, "c0=") + "/" + extract_word_after(token, "c1=");
        } else if (token.rfind("PrivateExchangeKeep", 0) == 0) {
            label += " keep " + extract_word_after(token, "c0=") + "/" + extract_word_after(token, "c1=");
        }
        pos = end + 1;
    }
    return label.empty() ? chain : label;
}

std::string public_history_json(const std::string& debug_label) {
    const std::string chain = extract_between(debug_label, "\npublic=", "\nprivate=");
    std::ostringstream out;
    out << "[";
    bool first = true;
    std::size_t pos = 0;
    while ((pos = chain.find("[", pos)) != std::string::npos) {
        const std::size_t end = chain.find("]", pos);
        if (end == std::string::npos) break;
        const std::string token = chain.substr(pos + 1, end - pos - 1);
        const std::string action = extract_word_after(token, "a=");
        if (!action.empty()) {
            if (!first) out << ",";
            first = false;
            out << "{\"player\":" << extract_int_after(token, "p=")
                << ",\"action\":\"" << action << "\"}";
        }
        pos = end + 1;
    }
    out << "]";
    return out.str();
}

int public_history_len(const std::string& history_json) {
    int count = 0;
    std::size_t pos = 0;
    while ((pos = history_json.find("\"action\"", pos)) != std::string::npos) {
        ++count; pos += 8;
    }
    return count;
}

// ── JSON helpers ──────────────────────────────────────────────────────────────

const char* action_color(Action action) {
    switch (action) {
        case Action::Income: return "#16a34a";
        case Action::ForeignAid: return "#0ea5e9";
        case Action::Tax: return "#9333ea";
        case Action::Steal: return "#d97706";
        case Action::Exchange: return "#0891b2";
        case Action::Assassinate: return "#f97316";
        case Action::Coup: return "#7c3aed";
        case Action::Allow: return "#6b7280";
        case Action::Challenge: return "#dc2626";
        case Action::BlockForeignAidDuke: return "#2563eb";
        case Action::BlockStealCaptain: return "#0f766e";
        case Action::BlockStealAmbassador: return "#14b8a6";
        case Action::BlockAssassinateContessa: return "#be185d";
        case Action::RevealSlot0:
        case Action::RevealSlot1: return "#64748b";
        case Action::LoseSlot0:
        case Action::LoseSlot1: return "#374151";
        case Action::Keep0:
        case Action::Keep1:
        case Action::Keep2:
        case Action::Keep3:
        case Action::Keep01:
        case Action::Keep02:
        case Action::Keep03:
        case Action::Keep12:
        case Action::Keep13:
        case Action::Keep23: return "#0284c7";
        case Action::ClaimMate: return "#111827";
        case Action::Count: return "#111827";
    }
    return "#111827";
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(ch); break;
        }
    }
    return out;
}

using StratArray = std::array<CfrValue, static_cast<std::size_t>(Action::Count)>;

std::string actions_json(const std::vector<Action>& actions, const StratArray& average) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (Action action : actions) {
        if (!first) out << ",";
        first = false;
        const std::size_t idx = static_cast<std::size_t>(action);
        out << "{\"name\":\"" << action_name(action) << "\","
            << "\"frequency\":" << std::fixed << std::setprecision(6) << average[idx] << ","
            << "\"color\":\"" << action_color(action) << "\"}";
    }
    out << "]";
    return out.str();
}

std::string aggregate_actions_json(const std::vector<const InfosetNode*>& rows) {
    std::array<double, static_cast<std::size_t>(Action::Count)> weighted{};
    weighted.fill(0.0);
    double total = 0.0;
    ActionMask legal_mask = 0;
    for (const InfosetNode* row : rows) {
        const double weight = strategy_weight(*row);
        const StratArray average = row->average_strategy();
        total += weight;
        legal_mask |= row->legal_mask;
        for (Action action : actions_from_mask(row->legal_mask))
            weighted[static_cast<std::size_t>(action)] += weight * average[static_cast<std::size_t>(action)];
    }
    StratArray aggregate{};
    aggregate.fill(0.0F);
    for (Action action : actions_from_mask(legal_mask)) {
        const std::size_t idx = static_cast<std::size_t>(action);
        aggregate[idx] = static_cast<CfrValue>(total > 0.0 ? weighted[idx] / total : 0.0);
    }
    return actions_json(actions_from_mask(legal_mask), aggregate);
}

// ── Export ────────────────────────────────────────────────────────────────────

void write_json_export(const CfrTrainer& trainer, const TrainingStats& stats,
                       const std::string& output_path, const ObservationStore& store) {
    struct Spot {
        int public_id{0};
        int player{0};
        std::string history_json;
        int history_len{0};
        std::vector<const InfosetNode*> rows;
    };

    std::map<std::string, Spot> spots;
    for (const auto& [key, node] : trainer.nodes()) {
        const std::string debug_label = regenerate_debug_label(node.key, store);
        Spot parsed;
        parsed.public_id    = extract_int_after(debug_label, "key public=");
        parsed.player       = extract_int_after(debug_label, "player=");
        parsed.history_json = public_history_json(debug_label);
        parsed.history_len  = public_history_len(parsed.history_json);

        const std::string spot_key = std::to_string(parsed.public_id) + "|" +
                                     std::to_string(parsed.player) + "|" +
                                     std::to_string(node.legal_mask);
        auto [it, inserted] = spots.try_emplace(spot_key);
        if (inserted) it->second = parsed;
        it->second.rows.push_back(&node);
    }

    std::vector<Spot> ordered_spots;
    ordered_spots.reserve(spots.size());
    for (const auto& item : spots) ordered_spots.push_back(item.second);
    std::sort(ordered_spots.begin(), ordered_spots.end(), [](const Spot& lhs, const Spot& rhs) {
        if (lhs.history_len != rhs.history_len) return lhs.history_len < rhs.history_len;
        double lv = 0.0, rv = 0.0;
        for (const InfosetNode* r : lhs.rows) lv += strategy_weight(*r);
        for (const InfosetNode* r : rhs.rows) rv += strategy_weight(*r);
        return lv > rv;
    });

    std::ofstream out(output_path);
    if (!out) throw std::runtime_error("could not open JSON output path");

    out << "{\n  \"stats\": {\"iterations\": " << stats.iterations
        << ", \"infosets\": " << stats.infosets << ", \"avgUtilityP0\": ";
    if (stats.iterations > 0)
        out << std::fixed << std::setprecision(6) << stats.utility0_sum / stats.iterations;
    else out << "0";
    out << "},\n";

    out << "  \"actions\": [";
    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        if (i > 0) out << ",";
        const Action a = static_cast<Action>(i);
        out << "{\"name\":\"" << action_name(a) << "\",\"color\":\"" << action_color(a) << "\"}";
    }
    out << "],\n  \"spots\": [\n";

    for (std::size_t i = 0; i < ordered_spots.size(); ++i) {
        const Spot& spot = ordered_spots[i];
        double spot_visits = 0.0;
        for (const InfosetNode* row : spot.rows) spot_visits += strategy_weight(*row);

        out << "    {\n"
            << "      \"id\": \"spot-" << i << "\",\n"
            << "      \"label\": \"P" << spot.player << "\",\n"
            << "      \"player\": " << spot.player << ",\n"
            << "      \"history\": " << spot.history_json << ",\n"
            << "      \"visits\": " << std::fixed << std::setprecision(3) << spot_visits << ",\n"
            << "      \"availableActions\": " << aggregate_actions_json(spot.rows) << ",\n"
            << "      \"holdings\": [\n";

        std::map<std::string, std::vector<const InfosetNode*>> holdings;
        for (const InfosetNode* row : spot.rows) {
            const std::string lbl = regenerate_debug_label(row->key, store);
            holdings[private_label(lbl)].push_back(row);
        }
        std::size_t hi = 0;
        for (const auto& [hlabel, hrows] : holdings) {
            double hv = 0.0;
            for (const InfosetNode* row : hrows) hv += strategy_weight(*row);
            if (hi++ > 0) out << ",\n";
            out << "        {\"card\": \""    << json_escape(hlabel) << "\","
                << " \"available\": true,"
                << " \"key\": \""             << spot.public_id << ":" << json_escape(hlabel) << "\","
                << " \"label\": \""           << json_escape(hlabel) << "\","
                << " \"visits\": "            << std::fixed << std::setprecision(3) << hv << ","
                << " \"actions\": "           << aggregate_actions_json(hrows) << "}";
        }
        out << "\n      ]\n    }";
        if (i + 1 < ordered_spots.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}

void write_json_export_atomic(const CfrTrainer& trainer, const TrainingStats& stats,
                              const std::string& output_path, const ObservationStore& store) {
    const std::string temp_path = output_path + ".tmp";
    write_json_export(trainer, stats, temp_path, store);
    if (std::rename(temp_path.c_str(), output_path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        throw std::runtime_error("could not replace JSON output path");
    }
}

void write_stats_atomic(const TrainingStats& stats, const std::string& output_path,
                        std::chrono::steady_clock::time_point start_time) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
    const double avg_utility = stats.iterations > 0
        ? stats.utility0_sum / static_cast<double>(stats.iterations) : 0.0;
    const double iter_per_sec = elapsed_sec > 0.0
        ? static_cast<double>(stats.iterations) / elapsed_sec : 0.0;
    const std::string temp_path = output_path + ".tmp";
    {
        std::ofstream out(temp_path);
        if (!out) throw std::runtime_error("could not open stats output path");
        out << "{\n"
            << "  \"iterations\": "   << stats.iterations << ",\n"
            << "  \"infosets\": "     << stats.infosets   << ",\n"
            << "  \"newInfosets\": "  << stats.new_infosets << ",\n"
            << "  \"newInfosetsPerIter\": "
            << std::fixed << std::setprecision(3) << stats.new_infosets_per_iter << ",\n"
            << "  \"avgUtilityP0\": " << std::setprecision(6) << avg_utility << ",\n"
            << "  \"elapsedSec\": "   << std::setprecision(3) << elapsed_sec << ",\n"
            << "  \"iterPerSec\": "   << std::setprecision(3) << iter_per_sec << "\n"
            << "}\n";
    }
    if (std::rename(temp_path.c_str(), output_path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        throw std::runtime_error("could not replace stats output path");
    }
}

void print_node(const InfosetNode& node, const ObservationStore& store) {
    const std::string label = regenerate_debug_label(node.key, store);
    const StratArray average = node.average_strategy();
    const std::vector<Action> actions = actions_from_mask(node.legal_mask);
    std::cout << label
              << "\nvisits=" << std::fixed << std::setprecision(1)
              << strategy_weight(node) << " actions:";
    for (Action action : actions)
        std::cout << " " << action_name(action) << "="
                  << std::setprecision(4) << average[static_cast<std::size_t>(action)];
    std::cout << "\n";
}

void log_progress(std::ostream& out, const TrainingStats& stats,
                  std::chrono::steady_clock::time_point start_time) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
    const double avg_utility = stats.iterations > 0
        ? stats.utility0_sum / static_cast<double>(stats.iterations) : 0.0;
    const double iter_per_sec = elapsed_sec > 0.0
        ? static_cast<double>(stats.iterations) / elapsed_sec : 0.0;
    out << "iter=" << stats.iterations
        << " infosets=" << stats.infosets
        << " avg_utility_p0=" << std::fixed << std::setprecision(6) << avg_utility
        << " elapsed_sec=" << std::setprecision(3) << elapsed_sec
        << " iter_per_sec=" << std::setprecision(3) << iter_per_sec << "\n";
    out.flush();
}

void run_training(CfrTrainer& trainer, TrainingStats& stats, int64_t iterations, bool infinite,
                  int log_every, std::ostream* log_stream, int checkpoint_json_every,
                  const std::string& checkpoint_json_path, int stats_every,
                  const std::string& stats_path, int mem_every,
                  const ObservationStore& store) {
    const auto start_time = std::chrono::steady_clock::now();
    std::size_t next_stats_infosets = stats_every > 0 ? static_cast<std::size_t>(stats_every) : 0;
    trainer.set_node_creation_callback([&](std::size_t infosets) {
        if (stats_every <= 0) return;
        if (infosets < next_stats_infosets) return;
        stats.infosets = infosets;
        write_stats_atomic(stats, stats_path, start_time);
        do { next_stats_infosets += static_cast<std::size_t>(stats_every); }
        while (infosets >= next_stats_infosets);
    });

    while (!stop_requested && (infinite || stats.iterations < iterations)) {
        const int traverser = static_cast<int>(stats.iterations % kPlayers);
        const std::size_t before_infosets = trainer.nodes().size();

        const CfrValue utility = trainer.run_iteration();

        const std::size_t after_infosets = trainer.nodes().size();

        stats.utility0_sum += traverser == 0 ? utility : -utility;
        ++stats.iterations;

        stats.last_infosets = before_infosets;
        stats.infosets = after_infosets;
        stats.new_infosets = after_infosets - before_infosets;
        stats.new_infosets_per_iter = static_cast<double>(stats.new_infosets);

        if (log_every > 0 && stats.iterations % log_every == 0)
            log_progress(log_stream != nullptr ? *log_stream : std::cerr, stats, start_time);
        if (mem_every > 0 && stats.iterations % mem_every == 0)
            log_mem(log_stream != nullptr ? *log_stream : std::cerr, trainer);
        if (checkpoint_json_every > 0 && stats.iterations % checkpoint_json_every == 0)
            write_json_export_atomic(trainer, stats, checkpoint_json_path, store);
        if (stats_every > 0 && stats.iterations % stats_every == 0)
            write_stats_atomic(stats, stats_path, start_time);
    }

    stats.infosets = trainer.nodes().size();
    if (stop_requested && log_every > 0)
        log_progress(log_stream != nullptr ? *log_stream : std::cerr, stats, start_time);
    if (mem_every > 0)
        log_mem(log_stream != nullptr ? *log_stream : std::cerr, trainer);
    if (stats_every > 0)
        write_stats_atomic(stats, stats_path, start_time);
    trainer.set_node_creation_callback(nullptr);
}

} // namespace

int main(int argc, char** argv) {
    try {
        int64_t iterations = 100;
        uint32_t seed = 1;
        int print_limit = 20;
        int max_2v2_public_actions = 20;
        int max_non_2v2_public_actions = 20;
        std::string json_output_path;
        std::string log_output_path;
        std::string stats_output_path;
        int log_every = 0;
        int checkpoint_json_every = 0;
        int stats_every = 0;
        int mem_every = 0;
        bool infinite = false;

        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);

        int positional = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--json") {
                if (i + 1 >= argc) throw std::invalid_argument("--json requires an output path");
                json_output_path = argv[++i]; continue;
            }
            if (arg == "--log") {
                if (i + 1 >= argc) throw std::invalid_argument("--log requires an output path");
                log_output_path = argv[++i]; continue;
            }
            if (arg == "--log-every") {
                if (i + 1 >= argc) throw std::invalid_argument("--log-every requires an iteration count");
                log_every = parse_int_arg(argv[++i], "log interval");
                if (log_every <= 0) throw std::invalid_argument("log interval must be positive");
                continue;
            }
            if (arg == "--checkpoint-json-every") {
                if (i + 1 >= argc) throw std::invalid_argument("--checkpoint-json-every requires an iteration count");
                checkpoint_json_every = parse_int_arg(argv[++i], "checkpoint JSON interval");
                if (checkpoint_json_every <= 0) throw std::invalid_argument("checkpoint JSON interval must be positive");
                continue;
            }
            if (arg == "--stats") {
                if (i + 1 >= argc) throw std::invalid_argument("--stats requires an output path");
                stats_output_path = argv[++i]; continue;
            }
            if (arg == "--stats-every") {
                if (i + 1 >= argc) throw std::invalid_argument("--stats-every requires an infoset count");
                stats_every = parse_int_arg(argv[++i], "stats infoset interval");
                if (stats_every <= 0) throw std::invalid_argument("stats interval must be positive");
                continue;
            }
            if (arg == "--mem-every") {
                if (i + 1 >= argc) throw std::invalid_argument("--mem-every requires an iteration count");
                mem_every = parse_int_arg(argv[++i], "mem poll interval");
                if (mem_every <= 0) throw std::invalid_argument("mem poll interval must be positive");
                continue;
            }
            if (arg == "--infinite") { infinite = true; continue; }
            ++positional;
            if (positional == 1) {
                if (is_infinite_arg(arg)) { infinite = true; iterations = std::numeric_limits<int64_t>::max(); }
                else iterations = parse_int_arg(argv[i], "iteration count");
            }
            else if (positional == 2) seed = static_cast<uint32_t>(parse_int_arg(argv[i], "seed"));
            else if (positional == 3) print_limit = parse_int_arg(argv[i], "print limit");
            else if (positional == 4) {
                max_2v2_public_actions = parse_int_arg(argv[i], "max 2v2 public actions");
                max_non_2v2_public_actions = max_2v2_public_actions;
            }
            else if (positional == 5) max_non_2v2_public_actions = parse_int_arg(argv[i], "max non-2v2 public actions");
            else throw std::invalid_argument("too many positional arguments");
        }
        if (positional >= 4 && positional < 5) max_non_2v2_public_actions = max_2v2_public_actions;
        if (checkpoint_json_every > 0 && json_output_path.empty())
            throw std::invalid_argument("--checkpoint-json-every requires --json path");
        if (stats_every > 0 && stats_output_path.empty())
            throw std::invalid_argument("--stats-every requires --stats path");
        if (!stats_output_path.empty() && stats_every == 0)
            throw std::invalid_argument("--stats requires --stats-every");

        std::ofstream log_file;
        std::ostream* log_stream = nullptr;
        if (!log_output_path.empty()) {
            log_file.open(log_output_path);
            if (!log_file) throw std::runtime_error("could not open log output path");
            log_stream = &log_file;
        } else if (log_every > 0 || mem_every > 0) {
            log_stream = &std::cerr;
        }

        // The ObservationStore is created here and passed into the trainer so
        // that solver.cpp can access it for label regeneration at export time.
        CfrTrainer trainer(seed, max_2v2_public_actions, max_non_2v2_public_actions);
        const ObservationStore& obs_store = trainer.observation_store();

        TrainingStats stats;
        run_training(trainer, stats, iterations, infinite, log_every, log_stream,
                    checkpoint_json_every, json_output_path, stats_every, stats_output_path,
                    mem_every, obs_store);

        if (!json_output_path.empty())
            write_json_export_atomic(trainer, stats, json_output_path, obs_store);

        std::cout << "iterations=" << stats.iterations << " infosets=" << stats.infosets;
        if (stats.iterations > 0)
            std::cout << " avg_utility_p0=" << std::fixed << std::setprecision(6)
                      << stats.utility0_sum / stats.iterations;
        std::cout << "\n";

        std::vector<const InfosetNode*> nodes;
        nodes.reserve(trainer.nodes().size());
        for (const auto& item : trainer.nodes()) nodes.push_back(&item.second);
        std::sort(nodes.begin(), nodes.end(), [](const InfosetNode* lhs, const InfosetNode* rhs) {
            return strategy_weight(*lhs) > strategy_weight(*rhs);
        });
        const int count = std::min(print_limit, static_cast<int>(nodes.size()));
        for (int i = 0; i < count; ++i)
            print_node(*nodes[static_cast<std::size_t>(i)], obs_store);

    } catch (const std::exception& error) {
        std::cerr << "solver error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}