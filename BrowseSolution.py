import re

INITIAL_HOLDINGS = {'11', '12', '13', '14', 
                '22', '23', '24',
                '33', '34', '44'}

def display_solution(solutions, game_tree):
    history = ""
    while True:
        print("\nGAME HISTORY: ", history)
        matching_keys = [key for key in game_tree if re.fullmatch(fr'^\d+{re.escape(history)}$', key)]
        if not history:
            matching_keys = list(INITIAL_HOLDINGS)

        if not matching_keys: 
            print("Terminal or Unexplored.")
            break

        for key in matching_keys:
            print(key, end=": ")
            if not solutions.get(key, None):
                print("No Solution")
            else:
                solutions[key].print_average_strategy()
                print()

        ac = input("\nNext action: (e.g. 'In'): ")

        history += ac