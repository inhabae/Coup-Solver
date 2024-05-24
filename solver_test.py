import KuhnCoupSolver as KSolver

# Testing InfoSet class
### Testing __init__()
possible_actions = []
infoset = KSolver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 0)
assert(infoset.num_actions == 0)
assert(len(infoset.strategy_sum) == 0)
assert(len(infoset.action_names) == 0)

possible_actions.append(KSolver.Action.INCOME)
infoset = KSolver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 1)
assert(infoset.num_actions == 1)
assert(len(infoset.strategy_sum) == 1)
assert(len(infoset.action_names) == 1)

possible_actions.append(KSolver.Action.ASSASSINATE)
infoset = KSolver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 2)
assert(infoset.num_actions == 2)
assert(len(infoset.strategy_sum) == 2)
assert(len(infoset.action_names) == 2)

# Testing KuhnCoup class
### Testing is_terminal()
assert (not KSolver.KuhnCoup.is_terminal([]))
assert (not KSolver.KuhnCoup.is_terminal([KSolver.Action.INCOME]))
assert (not KSolver.KuhnCoup.is_terminal([KSolver.Action.INCOME, KSolver.Action.INCOME]))
assert (not KSolver.KuhnCoup.is_terminal([KSolver.Action.INCOME, KSolver.Action.ASSASSINATE]))
assert (not KSolver.KuhnCoup.is_terminal([
    KSolver.Action.INCOME, KSolver.Action.ASSASSINATE,
    KSolver.Action.BLOCK_ASSASSINATE]))
assert (not KSolver.KuhnCoup.is_terminal([
    KSolver.Action.INCOME, KSolver.Action.ASSASSINATE, 
    KSolver.Action.BLOCK_ASSASSINATE, KSolver.Action.PASS, KSolver.Action.INCOME]))
assert (KSolver.KuhnCoup.is_terminal([
    KSolver.Action.INCOME, KSolver.Action.ASSASSINATE,
    KSolver.Action.BLOCK_ASSASSINATE, KSolver.Action.CHALLENGE]))
assert (KSolver.KuhnCoup.is_terminal([
    KSolver.Action.INCOME, KSolver.Action.INCOME,
    KSolver.Action.COUP]))
assert (KSolver.KuhnCoup.is_terminal([
    KSolver.Action.INCOME, KSolver.Action.INCOME,
    KSolver.Action.INCOME, KSolver.Action.COUP]))


### Testing get_payoff()
history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME,
    KSolver.Action.INCOME, KSolver.Action.COUP
]
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 0))

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME,
    KSolver.Action.COUP
]
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 1))

history = [KSolver.Action.ASSASSINATE, KSolver.Action.PASS]
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 0))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 0))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 0))

history = [KSolver.Action.INCOME, KSolver.Action.ASSASSINATE, KSolver.Action.PASS]
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 1))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 1))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 1))

history = [KSolver.Action.ASSASSINATE, KSolver.Action.CHALLENGE]
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 0))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CONTESSA'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'CONTESSA'], 0))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'ASSASSIN'], 0))

history = [KSolver.Action.INCOME, KSolver.Action.ASSASSINATE, KSolver.Action.CHALLENGE]
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'ASSASSIN'], 1))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CONTESSA'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN'], 1))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'CONTESSA'], 1))

### Testing get_possible_actions()
history = []
assert([KSolver.Action.INCOME, KSolver.Action.ASSASSINATE] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Start of game
history = [KSolver.Action.INCOME]
assert([KSolver.Action.INCOME, KSolver.Action.ASSASSINATE] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Free turn with 1 coin
history = [KSolver.Action.ASSASSINATE]
assert([KSolver.Action.PASS, KSolver.Action.CHALLENGE, KSolver.Action.BLOCK_ASSASSINATE] == 
       KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # vs Assassination
history = [KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE]
assert([KSolver.Action.PASS, KSolver.Action.CHALLENGE] == 
       KSolver.KuhnCoup.get_possible_actions(history, 0, True)) # vs Assassination Block
history = [KSolver.Action.INCOME, KSolver.Action.INCOME, KSolver.Action.INCOME]
assert([KSolver.Action.INCOME, KSolver.Action.ASSASSINATE, KSolver.Action.COUP] == 
       KSolver.KuhnCoup.get_possible_actions(history, 2, True)) # Free turn with 2 coins
history = [KSolver.Action.INCOME, KSolver.Action.INCOME, KSolver.Action.INCOME, KSolver.Action.INCOME]
assert([KSolver.Action.COUP] == KSolver.KuhnCoup.get_possible_actions(history, 3, True)) # Free turn with 3 coins
history = [KSolver.Action.INCOME, KSolver.Action.INCOME, KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, KSolver.Action.PASS, KSolver.Action.INCOME]
assert([KSolver.Action.INCOME, KSolver.Action.ASSASSINATE] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Free turn with 1 coin

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME, 
    KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, 
    KSolver.Action.PASS, KSolver.Action.INCOME, KSolver.Action.INCOME
]
assert([KSolver.Action.COUP] == KSolver.KuhnCoup.get_possible_actions(history, 3, True)) # Free turn with 3 coins

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME, 
    KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, 
    KSolver.Action.PASS, KSolver.Action.INCOME, KSolver.Action.ASSASSINATE,
    KSolver.Action.BLOCK_ASSASSINATE
]
assert([KSolver.Action.CHALLENGE] == KSolver.KuhnCoup.get_possible_actions(history, 0, False)) # vs Second Assassination Block

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME, 
    KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, 
    KSolver.Action.PASS, KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE,
]
assert([KSolver.Action.PASS, KSolver.Action.CHALLENGE] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # vs Second Assassination Block



# Testing KuhnCFRTrainer Class
### Testing __init__()
trainer = KSolver.KuhnCFRTrainer()
assert(len(trainer.infoset_map) == 0)

### Testing get_information_set()
history = []
coins = [1,1]
trainer.get_information_set('CONTESSA',coins, history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 1)
assert('CONTESSA11' in trainer.infoset_map.keys())

history = [KSolver.Action.INCOME]
coins = [2,1]
trainer.get_information_set('ASSASSIN',coins, history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 2)
assert('ASSASSIN210' in trainer.infoset_map.keys())

trainer.get_information_set('ASSASSIN',coins, history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 2)
assert('ASSASSIN210' in trainer.infoset_map.keys())

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME, 
    KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, 
    KSolver.Action.PASS, KSolver.Action.INCOME, KSolver.Action.INCOME
]
coins = [1,2]
trainer.get_information_set('ASSASSIN',coins, history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 3)
assert('ASSASSIN120015300' in trainer.infoset_map.keys())

history = [
    KSolver.Action.INCOME, KSolver.Action.INCOME, 
    KSolver.Action.ASSASSINATE, KSolver.Action.BLOCK_ASSASSINATE, 
    KSolver.Action.PASS, KSolver.Action.INCOME, KSolver.Action.INCOME
]
coins = [1,2]
trainer.get_information_set('CONTESSA',coins, history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 4)
assert('CONTESSA120015300' in trainer.infoset_map.keys())


print("No Assertion Errors")