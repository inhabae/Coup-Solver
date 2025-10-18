#include "trainer.hpp"
#include <iostream>

int main() {
    Trainer trainer;

    auto valid_histories = trainer.find_all_2v2_terminals(2);

    std::cout << "Found " << valid_histories.size() << " total valid histories (terminals + prefixes)" << std::endl;

    trainer.train(1);  // Reduce for debugging

    return 0;
}
