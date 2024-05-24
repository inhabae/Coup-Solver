import pickle


ACTION_NAMES = ['INCOME', 'ASSASSINATE', 'COUP', 'PASS', 'CHALLENGE', 'BLOCK ASSASSINATION']

class SolutionDisplay():
    def __init__(self, solutions):
        self.solutions = solutions
        self.current_history = ''
        
    def move_forward(self, next_action):
        self.current_history += next_action

    def move_backward(self):
        self.current_history = self.current_history[:-1]

    def display_solution(self):
        for card_solution in self.solutions[self.current_history]:
            for card, strategies in card_solution.items():
                print(f"<{card}>")
                for action_name, action_freq in strategies.items():
                    print(f"\t{ACTION_NAMES[action_name]}: {action_freq}")

    def round_solutions(self, ndigits):
        for card_solutions in self.solutions.values():
            for card_solution in card_solutions:
                for strategies in card_solution.values():
                    for action_name, action_freq in strategies.items():
                        strategies[action_name] = round(action_freq, ndigits)


if __name__ == "__main__":
    with open('solutions.pkl', 'rb') as file:
        solutions = pickle.load(file)
    sd = SolutionDisplay(solutions)
    sd.round_solutions(3) # strategies are rounded to 3 decimal places

    while True:
        print("Kuhn Coup Solver")
        print("---History--------------------")
        print(sd.current_history)
        print("------------------------------")
        print("---Available Actions----------")
        available_actions = []
        for strategy in sd.solutions[sd.current_history]:
            for action_strategy in strategy.values():
                for action in action_strategy.keys():
                    if action not in available_actions: 
                        available_actions.append(action)

        print([ACTION_NAMES[action] for action in available_actions])
        print("------------------------------")
        print("---Solutions------------------")
        sd.display_solution()
        print("------------------------------")
        
        print("Enter an action you would like to take (one length string).")
        user_input = input("Or enter 'z' to undo the most recent action: ")

        assert len(user_input) == 1

        if user_input == 'z':
            sd.move_backward()
        else:
            sd.move_forward(user_input)


        print()
        print()


        #TODO: stop at terminal node instead of error