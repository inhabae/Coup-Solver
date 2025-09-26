#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

enum class Action {
    GAME_START,
    INCOME,
    TAX,
    ASSASSINATE,
    COUP,
    BLOCK_ASSASSINATE,
    CHALLENGE,
    PASS_BLOCK,
    LOSE_CARD,
    SHOW_CARD
};

static const std::unordered_map<Action, std::string> ACTION_NAMES = {
    {Action::GAME_START, "GAME_START"},
    {Action::INCOME, "INCOME"},
    {Action::TAX, "TAX"},
    {Action::ASSASSINATE, "ASSASSINATE"},
    {Action::COUP, "COUP"},
    {Action::BLOCK_ASSASSINATE, "BLOCK_ASSASSINATE"},
    {Action::CHALLENGE, "CHALLENGE"},
    {Action::PASS_BLOCK, "PASS_BLOCK"},
    {Action::LOSE_CARD, "LOSE_CARD"},
    {Action::SHOW_CARD, "SHOW_CARD"}
};

// Function to load strategies from binary file
std::unordered_map<std::string, std::unordered_map<Action, double>> load_strategies_binary(const std::string& filename) {
    std::unordered_map<std::string, std::unordered_map<Action, double>> strat;
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return strat;
    }
    
    size_t n;
    file.read(reinterpret_cast<char*>(&n), sizeof(n));
    while (n--) {
        size_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string key(len, '\0');
        file.read(&key[0], len);
        size_t na;
        file.read(reinterpret_cast<char*>(&na), sizeof(na));
        for (size_t i = 0; i < na; ++i) {
            Action a;
            double p;
            file.read(reinterpret_cast<char*>(&a), sizeof(a));
            file.read(reinterpret_cast<char*>(&p), sizeof(p));
            strat[key][a] = p;
        }
    }
    file.close();
    return strat;
}

int main() {
    std::string filename = "strategies.bin";
    auto strategies = load_strategies_binary(filename);
    
    if (strategies.empty()) {
        std::cout << "No strategies loaded. Check if the file exists and is valid." << std::endl;
        return 1;
    }

    std::string infoset_key;
    while (true) { 
        std::cout << "\nEnter the Information Set key: ";
        std::getline(std::cin, infoset_key);
        auto it = strategies.find(infoset_key);
        if (it == strategies.end()) {
            std::cout << "Information set key '" << infoset_key << "' not found." << std::endl;
            continue;
        }

        std::cout << "\nActions and their strategy frequencies for " << infoset_key << ":\n";
        for (const auto& action_pair : it->second) {
            auto action_it = ACTION_NAMES.find(action_pair.first);
            if (action_it != ACTION_NAMES.end()) {
                std::cout << action_it->second << ": " << action_pair.second << std::endl;
            } else {
                std::cout << "Unknown Action: " << static_cast<int>(action_pair.first) << ": " << action_pair.second << std::endl;
            }
        }

    }
   

    return 0;
}