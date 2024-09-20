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
assert (not KSolver.KuhnCoup.is_terminal('0'))
assert (not KSolver.KuhnCoup.is_terminal('00'))
assert (not KSolver.KuhnCoup.is_terminal('01'))
assert (not KSolver.KuhnCoup.is_terminal('015'))
assert (not KSolver.KuhnCoup.is_terminal('01531'))
assert (KSolver.KuhnCoup.is_terminal('0154'))
assert (KSolver.KuhnCoup.is_terminal('002'))
assert (KSolver.KuhnCoup.is_terminal('0002'))


### Testing get_payoff()
history = '0002'
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))

history = '002'
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))

history = '13'
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))

history = '013'
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))

history = '14'
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CONTESSA']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'CONTESSA']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'ASSASSIN']))

history = '014'
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'ASSASSIN']))
assert(1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'ASSASSIN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CIVILIAN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['ASSASSIN', 'CONTESSA']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CONTESSA', 'CIVILIAN']))
assert(-1 == KSolver.KuhnCoup.get_payoff(history, ['CIVILIAN', 'CONTESSA']))

### Testing get_possible_actions()
history = ''
assert([KSolver.Action.INCOME.value, KSolver.Action.ASSASSINATE.value] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Start of game
history = '0'
assert([KSolver.Action.INCOME.value, KSolver.Action.ASSASSINATE.value] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Free turn with 1 coin
history = '1'
assert([KSolver.Action.PASS.value, KSolver.Action.CHALLENGE.value, KSolver.Action.BLOCK_ASSASSINATE.value] == 
       KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # vs Assassination
history = '15'
assert([KSolver.Action.PASS.value, KSolver.Action.CHALLENGE.value] == 
       KSolver.KuhnCoup.get_possible_actions(history, 0, True)) # vs Assassination Block
history = '000'
assert([KSolver.Action.INCOME.value, KSolver.Action.ASSASSINATE.value, KSolver.Action.COUP.value] == 
       KSolver.KuhnCoup.get_possible_actions(history, 2, True)) # Free turn with 2 coins
history = '0000'
assert([KSolver.Action.COUP.value] == KSolver.KuhnCoup.get_possible_actions(history, 3, True)) # Free turn with 3 coins
history = '001530'
assert([KSolver.Action.INCOME.value, KSolver.Action.ASSASSINATE.value] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # Free turn with 1 coin

history = '0015300'
assert([KSolver.Action.COUP.value] == KSolver.KuhnCoup.get_possible_actions(history, 3, True)) # 3 coins+ -> Coup

history = '00153015'
assert([KSolver.Action.CHALLENGE.value] == KSolver.KuhnCoup.get_possible_actions(history, 0, False)) # vs Second Assassination Block

history = '0015315'
assert([KSolver.Action.PASS.value, KSolver.Action.CHALLENGE.value] == KSolver.KuhnCoup.get_possible_actions(history, 1, True)) # vs Second Assassination Block



# Testing KuhnCFRTrainer Class
### Testing __init__()
trainer = KSolver.KuhnCFRTrainer()
assert(len(trainer.infoset_map) == 0)

### Testing get_information_set()
history = ''
coins = [1,1]
trainer.get_information_set('CONTESSA', history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 1)
assert('CONTESSA' in trainer.infoset_map.keys())

history = '0'
coins = [2,1]
trainer.get_information_set('ASSASSIN', history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 2)
assert('ASSASSIN0' in trainer.infoset_map.keys())

trainer.get_information_set('ASSASSIN', history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 2)
assert('ASSASSIN0' in trainer.infoset_map.keys())

history = '0015300'
coins = [1,2]
trainer.get_information_set('ASSASSIN', history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 3)
assert('ASSASSIN0015300' in trainer.infoset_map.keys())

history = '0015300'
coins = [1,2]
trainer.get_information_set('CONTESSA', history, KSolver.KuhnCoup.get_possible_actions(history,1,True))
assert(len(trainer.infoset_map) == 4)
assert('CONTESSA0015300' in trainer.infoset_map.keys())

# ### Testing is_p1_key()
# key = 'CONTESSA'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA00'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA01'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA15'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA13'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA000002'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)
# key = 'CONTESSA14'
# assert(KSolver.KuhnCoup.is_p1_key(key) == True)

# key = 'ASSASSIN0'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN001'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN002'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN1'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN000'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN015'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'ASSASSIN00154'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'CIVILIAN000'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)
# key = 'CIVILIAN013'
# assert(KSolver.KuhnCoup.is_p1_key(key) == False)

# # Testing MESFinder()
# ### Testing get_child_keys()
# all_keys = [
#     'CONTESSA0', 'CONTESSA00', 'CONETSSA000',
#     'CONTESSA01', 'CONTESSA001', 'CONETSSA0001',
#     'CONTESSA1', 'CONTESSA14', 'CONETSSA153',
#     'CONTESSA015', 'CONTESSA13', 'CONETSSA002',
#     'CONTESSA0002', 'CONTESSA00014', 'CONETSSA00154',
#     'ASSASSIN', 'ASSASSIN00', 'ASSASSIN000',
#     'ASSASSIN01', 'ASSASSIN001', 'ASSASSIN0001',
#     'ASSASSIN1', 'ASSASSIN14', 'ASSASSIN153',
#     'ASSASSIN015', 'ASSASSIN13', 'ASSASSIN002',
#     'ASSASSIN0002', 'ASSASSIN00014', 'ASSASSIN00154',
#     'CIVILIAN', 'CIVILIAN00', 'CIVILIAN000',
#     'CIVILIAN01', 'CIVILIAN001', 'CIVILIAN0001',
#     'CIVILIAN1', 'CIVILIAN14', 'CIVILIAN153',
#     'CIVILIAN015', 'CIVILIAN13', 'CIVILIAN002',
#     'CIVILIAN0002', 'CIVILIAN00014', 'CIVILIAN00154'
# ]
# mes_finder = KSolver.MESFinder({}, all_keys)
# assert(set(mes_finder.get_child_keys('CONTESSA0')) ==  {'ASSASSIN00', 'ASSASSIN01', 'CIVILIAN00', 'CIVILIAN01'})
# assert(set(mes_finder.get_child_keys('ASSASSIN01')) ==  {'CONTESSA015', 'CIVILIAN015'})


# ### Testing get_payoff_for_mes()
# cur_key = 'ASSASSIN0002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'ASSASSIN0002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'ASSASSIN002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'ASSASSIN00002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA00002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA0002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'CONTESSA002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA002'
# assert(-1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'CONTESSA0013'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'CONTESSA0013'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA0013'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA013'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'CONTESSA13'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, False))

# cur_key = 'CONTESSA0154'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, True))

# cur_key = 'CONTESSA00154'
# assert(1 == mes_finder.get_payoff_for_mes(cur_key, False))


print("No Assertion Errors")