#include "../coup/small_coup.hpp"
#include "../coup/small_coup_rebel.hpp"

#include <array>
#include <memory>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

std::vector<double> policy_vector(const small_coup::rebel::SearchResult& result) {
    return std::vector<double>(result.policy.begin(), result.policy.end());
}

std::vector<double> belief_vector(const small_coup::rebel::BeliefState& belief) {
    return std::vector<double>(belief.probabilities.begin(), belief.probabilities.end());
}

std::vector<small_coup::Deal> all_deals_vector() {
    const auto deals = small_coup::rebel::all_deals();
    return std::vector<small_coup::Deal>(deals.begin(), deals.end());
}

std::vector<small_coup::Action> legal_actions(const small_coup::GameState& state) {
    return small_coup::actions_from_mask(state.legal_actions());
}

small_coup::rebel::SearchResult resolve_heuristic(const small_coup::GameState& state, int player,
                                                  int iterations, int depth, uint32_t seed) {
    small_coup::rebel::HeuristicValueEvaluator evaluator;
    small_coup::rebel::DepthLimitedResolver resolver(iterations, depth, evaluator, seed);
    return resolver.resolve(state, player);
}

} // namespace

PYBIND11_MODULE(_small_coup_rebel, m) {
    m.doc() = "pybind11 bridge for small_coup ReBeL training and resolving";

    py::enum_<small_coup::Card>(m, "Card")
        .value("Assassin", small_coup::Card::Assassin)
        .value("Contessa", small_coup::Card::Contessa)
        .value("Civilian", small_coup::Card::Civilian)
        .value("None_", small_coup::Card::None);

    py::enum_<small_coup::Phase>(m, "Phase")
        .value("TurnAction", small_coup::Phase::TurnAction)
        .value("RespondToAssassinate", small_coup::Phase::RespondToAssassinate)
        .value("RespondToBlock", small_coup::Phase::RespondToBlock)
        .value("LoseLife", small_coup::Phase::LoseLife)
        .value("Terminal", small_coup::Phase::Terminal);

    py::enum_<small_coup::Action>(m, "Action")
        .value("Income", small_coup::Action::Income)
        .value("Assassinate", small_coup::Action::Assassinate)
        .value("Coup", small_coup::Action::Coup)
        .value("Allow", small_coup::Action::Allow)
        .value("BlockAssassinate", small_coup::Action::BlockAssassinate)
        .value("Challenge", small_coup::Action::Challenge)
        .value("LoseLife", small_coup::Action::LoseLife);

    py::class_<small_coup::Deal>(m, "Deal")
        .def(py::init<>())
        .def_readwrite("cards", &small_coup::Deal::cards)
        .def_readwrite("hidden", &small_coup::Deal::hidden);

    py::class_<small_coup::PublicEvent>(m, "PublicEvent")
        .def_readwrite("action", &small_coup::PublicEvent::action)
        .def_readwrite("player", &small_coup::PublicEvent::player);

    py::class_<small_coup::GameState>(m, "GameState")
        .def(py::init<>())
        .def(py::init<const small_coup::Deal&>())
        .def("is_terminal", &small_coup::GameState::is_terminal)
        .def("current_player", &small_coup::GameState::current_player)
        .def("phase", &small_coup::GameState::phase)
        .def("legal_actions", &legal_actions)
        .def("is_legal", &small_coup::GameState::is_legal)
        .def("apply", &small_coup::GameState::apply)
        .def("undo", &small_coup::GameState::undo)
        .def("utility", &small_coup::GameState::utility)
        .def("coins", &small_coup::GameState::coins)
        .def("lives", &small_coup::GameState::lives)
        .def("has_assassinated", &small_coup::GameState::has_assassinated)
        .def("card", &small_coup::GameState::card)
        .def("hidden_card", &small_coup::GameState::hidden_card)
        .def("public_history_size", &small_coup::GameState::public_history_size)
        .def("debug_string", &small_coup::GameState::debug_string);

    py::class_<small_coup::rebel::PublicState>(m, "PublicState")
        .def_readwrite("phase", &small_coup::rebel::PublicState::phase)
        .def_readwrite("current_player", &small_coup::rebel::PublicState::current_player)
        .def_readwrite("coins", &small_coup::rebel::PublicState::coins)
        .def_readwrite("lives", &small_coup::rebel::PublicState::lives)
        .def_readwrite("assassinated", &small_coup::rebel::PublicState::assassinated)
        .def_readwrite("legal_mask", &small_coup::rebel::PublicState::legal_mask)
        .def_readwrite("history", &small_coup::rebel::PublicState::history)
        .def("serialize", &small_coup::rebel::PublicState::serialize)
        .def("features", &small_coup::rebel::PublicState::features);

    py::class_<small_coup::rebel::BeliefState>(m, "BeliefState")
        .def("probability_sum", &small_coup::rebel::BeliefState::probability_sum)
        .def_property_readonly("probabilities", &belief_vector);

    py::class_<small_coup::rebel::SearchResult>(m, "SearchResult")
        .def_readwrite("value", &small_coup::rebel::SearchResult::value)
        .def_property_readonly("policy", &policy_vector);

    py::class_<small_coup::rebel::TrainingSample>(m, "TrainingSample")
        .def_readwrite("public_state", &small_coup::rebel::TrainingSample::public_state)
        .def_readwrite("belief", &small_coup::rebel::TrainingSample::belief)
        .def_readwrite("search_result", &small_coup::rebel::TrainingSample::search_result)
        .def_readwrite("player", &small_coup::rebel::TrainingSample::player)
        .def_readwrite("target_value", &small_coup::rebel::TrainingSample::target_value)
        .def("features", &small_coup::rebel::TrainingSample::features);

    m.attr("ACTION_COUNT") = static_cast<int>(small_coup::Action::Count);
    m.attr("DEAL_COUNT") = small_coup::rebel::kDealCount;
    m.def("card_name", &small_coup::card_name);
    m.def("action_name", &small_coup::action_name);
    m.def("phase_name", &small_coup::phase_name);
    m.def("all_deals", &all_deals_vector);
    m.def("public_state_from", &small_coup::rebel::public_state_from);
    m.def("belief_from_public_state", &small_coup::rebel::belief_from_public_state,
          py::arg("public_state"), py::arg("known_player") = -1, py::arg("known_card") = small_coup::Card::None);
    m.def("resolve_heuristic", &resolve_heuristic,
          py::arg("state"), py::arg("player"), py::arg("iterations") = 64, py::arg("depth") = 4,
          py::arg("seed") = 1);
    m.def("generate_training_samples", &small_coup::rebel::generate_training_samples,
          py::arg("samples"), py::arg("max_steps") = 16, py::arg("seed") = 1,
          py::arg("resolve_iterations") = 64, py::arg("resolve_depth") = 4);
}
