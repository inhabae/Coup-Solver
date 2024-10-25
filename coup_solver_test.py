import CoupSolver as Solver
import performAction as Pa
# Testing InfoSet class
### Testing __init__()
possible_actions = []
infoset = Solver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 0)
assert(infoset.num_actions == 0)
assert(len(infoset.strategy_sum) == 0)
assert(len(infoset.action_names) == 0)

possible_actions.append(Solver.Action.INCOME)
infoset = Solver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 1)
assert(infoset.num_actions == 1)
assert(len(infoset.strategy_sum) == 1)
assert(len(infoset.action_names) == 1)

possible_actions.append(Solver.Action.ASSASSINATE)
infoset = Solver.InfoSet(possible_actions)
assert(len(infoset.cumulative_regrets) == 2)
assert(infoset.num_actions == 2)
assert(len(infoset.strategy_sum) == 2)
assert(len(infoset.action_names) == 2)

# Testing KuhnCoup class
### Testing get_possible_actions()
gpa = Solver.Coup.get_possible_actions
history = []
assert(set(gpa(history, 2, ['CONTESSA', 'ASSASSIN'], 2, 2)) == {'In', 'Tx', 'S2', 'Fa'})
history = ['In', 'In']
assert(set(gpa(history, 3, ['CONTESSA', 'ASSASSIN'], 2, 3)) == {'In', 'Tx', 'S2', 'Fa', 'As'})
history = ['In', 'In', 'Tx']
assert(set(gpa(history, 3, ['CONTESSA', 'ASSASSIN'], 2, 6)) == {'In', 'Tx', 'S2', 'Fa', 'As', 'Ch'})
history = ['In', 'In', 'Tx', 'Tx']
assert(set(gpa(history, 6, ['CONTESSA', 'ASSASSIN'], 2, 6)) == {'In', 'Tx', 'S2', 'Fa', 'As', 'Ch'})
history = ['In', 'In', 'Tx', 'Tx', 'In', 'In']
assert(set(gpa(history, 7, ['CONTESSA', 'ASSASSIN'], 2, 7)) == {'In', 'Tx', 'S2', 'Fa', 'As', 'Co'})
history = ['In', 'In', 'As']
assert(set(gpa(history, 0, ['CONTESSA', 'ASSASSIN'], 2, 3)) == {'Ba', 'Ch', 'Lc', 'La'})
history = ['In', 'In', 'As', 'Lc']
assert(set(gpa(history, 3, ['ASSASSIN'], 2, 0)) == {'In', 'Tx', 'Fa', 'As'})
history = ['In', 'In', 'As', 'Ch']
assert(set(gpa(history, 0, ['ASSASSIN', 'CONTESSA'], 2, 3)) == {'Sh'})
history = ['In', 'In', 'As', 'Ch', 'Lc', 'As']
assert(set(gpa(history, 0, ['CONTESSA'], 2, 0)) == {'Ch', 'Ba'}) # Losing Contessa is dominated.
history = ['S2']
assert(set(gpa(history, 0, ['CONTESSA', 'ASSASSIN'], 2, 4)) == {'In', 'Tx', 'S2', 'Fa', 'Bs', 'Ch'})
history = ['In', 'In', 'As', 'Ch', 'Lc', 'As', 'Ba']
assert(set(gpa(history, 0, ['CONTESSA'], 2, 0)) == {'Ch', 'Pa'})

# 1/13/24 Game 1 vs Derek
history = ['Tx', 'Tx']
assert(set(gpa(history, 5, ['DUKE', 'ASSASSIN'], 2, 5)) == {'Ch','In','Fa','Tx','As','S2'})
history = ['Tx', 'Tx', 'Tx', 'Tx']
assert(set(gpa(history, 8, ['DUKE', 'ASSASSIN'], 2, 8)) == {'Ch','In','Fa','Tx','As','S2','Co'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','La','Co']
assert(set(gpa(history, 1, ['DUKE', 'ASSASSIN'], 1, 1)) == {'Ld', 'La'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','La','Co','La']
assert(set(gpa(history, 1, ['DUKE'], 1, 1)) == {'In', 'Tx', 'Fa', 'S1'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','La','Co','La', 'Tx', 'Tx']
assert(set(gpa(history, 4, ['DUKE'], 1, 4)) == {'In', 'Tx', 'Fa', 'S2', 'Ch', 'As'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','La','Co','La', 'Tx', 'Tx', 'Tx', 'As']
assert(set(gpa(history, 7, ['DUKE'], 1, 1)) == {'Ba', 'Ch'})

# 1/13/24 Game 5 vs Derek
history = ['Tx', 'Tx']
assert(set(gpa(history, 5, ['DUKE', 'CAPTAIN'], 2, 5)) == {'Ch','In','Fa','Tx','As','S2'})
history = ['Tx', 'Tx', 'Ch', 'Sh']
assert(set(gpa(history, 5, ['DUKE', 'CAPTAIN'], 2, 5)) == {'Ld', 'Lp'})
history = ['Tx', 'Tx', 'Ch', 'Sh', 'Ld']
assert(set(gpa(history, 5, ['CAPTAIN'], 2, 5)) == {'In', 'Fa', 'Tx', 'S2', 'As'})
history = ['Tx', 'Tx', 'Ch', 'Sh', 'Ld', 'S2', 'Bs']
assert(set(gpa(history, 5, ['CAPTAIN'], 2, 5)) == {'Ch', 'Pa'})

# 1/13/24 Game 8 vs Derek
history = ['Tx', 'Tx']
assert(set(gpa(history, 5, ['DUKE', 'CONTESSA'], 2, 5)) == {'Ch','In','Fa','Tx','As','S2'})
history = ['Tx', 'Tx', 'Tx', 'Tx']
assert(set(gpa(history, 8, ['DUKE', 'CONTSSA'], 2, 8)) == {'Ch','In','Fa','Tx','As','S2','Co'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','Ld','Co']
assert(set(gpa(history, 1, ['DUKE', 'CONTESSA'], 1, 1)) == {'Ld', 'Lc'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','Ld','Co', 'Ld']
assert(set(gpa(history, 1, ['CONTESSA'], 1, 1)) == {'In','Fa','Tx','S1'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','Ld','Co', 'Ld', 'Fa', 'Fa']
assert(set(gpa(history, 3, ['CONTESSA'], 3, 1)) == {'In','Fa','Tx','S1', 'As', 'Bf'})
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','Ld','Co', 'Ld', 'Fa', 'Fa', 'Fa', 'S2']
assert(set(gpa(history, 3, ['CONTESSA'], 1, 5)) == {'In','Fa','Tx','S2','As','Ch','Bs'}) 
history = ['Tx', 'Tx', 'Tx', 'Tx','Co','Ld','Co', 'Ld', 'Fa', 'Fa', 'Fa', 'S2','Bs','Ch']
assert(set(gpa(history, 3, ['CONTESSA'], 1, 5)) == {'Lc'})

# 1/13/24 Game 6 vs Derek
history = ['Tx', 'Tx', 'Tx', 'Tx', 'As']
assert(set(gpa(history, 8, ['CAPTAIN', 'CONTESSA'], 2, 5)) == {'Ch','Ba', 'Lp', 'Lc'})
history = ['Tx', 'Tx', 'Tx', 'Tx', 'As', 'Lp']
assert(set(gpa(history, 8, ['CONTESSA'], 2, 5)) == {'In','Fa','Co','Tx','S2','As'})
history = ['Tx', 'Tx', 'Tx', 'Tx', 'As', 'Lp', 'Co', 'Ld', 'As']
assert(set(gpa(history, 1, ['CONTESSA'], 1, 2)) == {'Ch', 'Ba'})

### Testing cfr()'s action-performing code
# 1/13/24 Game 1 vs Derek
new_history = []
deck = ['DUKE', 'DUKE', 'ASSASSIN', 'CONTESSA', 'CONTESSA', 
        'CONTESSA', 'CAPTAIN', 'CAPTAIN', 'CAPTAIN']
new_history = Pa.perform_action('Tx', [2,2], new_history, 0, ['DUKE', 'ASSASSIN'], deck)
new_history = Pa.perform_action('Tx', [5,2], new_history, 1, ['ASSASSIN', 'ASASSIN'], deck)
new_history = Pa.perform_action('Tx', [5,5], new_history, 0, ['DUKE', 'ASSASSIN'], deck)
new_history = Pa.perform_action('Tx', [8,5], new_history, 1, ['ASSASSIN', 'ASASSIN'], deck)
new_history = Pa.perform_action('Co', [8,8], new_history, 0, ['DUKE', 'ASSASSIN'], deck)
new_history = Pa.perform_action('La', [1,8], new_history, 1, ['ASSASSIN', 'ASASSIN'], deck)
new_history = Pa.perform_action('Co', [1,8], new_history, 1, ['ASASSIN'], deck)
new_history = Pa.perform_action('La', [1,1], new_history, 0, ['DUKE', 'ASSASSIN'], deck)
new_history = Pa.perform_action('Tx', [1,1], new_history, 0, ['DUKE'], deck)
new_history = Pa.perform_action('Tx', [4,1], new_history, 1, ['ASSASSIN'], deck)
new_history = Pa.perform_action('Tx', [4,4], new_history, 0, ['DUKE'], deck)
new_history = Pa.perform_action('As', [7,4], new_history, 1, ['ASSASSIN'], deck)
assert(set(gpa(new_history, 7, ['ASSASSIN'], 1, 1)) == {'Ch','Ba'}) 
new_history = Pa.perform_action('Ch', [7,1], new_history, 0, ['DUKE'], deck)
assert(set(gpa(new_history, 1, ['ASSASSIN'], 1, 7)) == {'Sh'}) 
new_history = Pa.perform_action('Sh', [7,1], new_history, 1, ['ASSASSIN'], deck)
assert(set(gpa(new_history, 7, ['DUKE'], 1, 1)) == {'Ld'}) 
new_history = Pa.perform_action('Ld', [7,1], new_history, 0, ['DUKE'], deck)
assert(Solver.Coup.is_terminal(len([]), len(['ASSASSIN'])))

# 1/13/24 Game 8 vs Derek
new_history = []
new_history = Pa.perform_action('Tx', [2,2], new_history, 0, ['DUKE', 'CONTESSA'], deck)
new_history = Pa.perform_action('Tx', [5,2], new_history, 1, ['DUKE', 'CAPTAIN'], deck)
new_history = Pa.perform_action('Tx', [5,5], new_history, 0, ['DUKE', 'CONTESSA'], deck)
new_history = Pa.perform_action('Tx', [8,5], new_history, 1, ['DUKE', 'CAPTAIN'], deck)
new_history = Pa.perform_action('Co', [8,8], new_history, 0, ['DUKE', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ld', [1,8], new_history, 1, ['DUKE', 'CAPTAIN'], deck)
new_history = Pa.perform_action('Co', [1,8], new_history, 1, ['CAPTAIN'], deck)
new_history = Pa.perform_action('Ld', [1,1], new_history, 0, ['DUKE', 'CONTESSA'], deck)
new_history = Pa.perform_action('Fa', [1,1], new_history, 0, ['CONTESSA'], deck)
new_history = Pa.perform_action('Fa', [3,1], new_history, 1, ['CAPTAIN'], deck)
new_history = Pa.perform_action('Fa', [3,3], new_history, 0, ['CONTESSA'], deck)
new_history = Pa.perform_action('S2', [5,3], new_history, 1, ['CAPTAIN'], deck)
new_history = Pa.perform_action('Bs', [3,5], new_history, 0, ['CONTESSA'], deck)
new_history = Pa.perform_action('Ch', [5,3], new_history, 1, ['CAPTAIN'], deck)
new_history = Pa.perform_action('Lc', [5,3], new_history, 0, ['CONTESSA'], deck)

# Double Assassination: Challenging real assassin
new_history = []
new_history = Pa.perform_action('In', [2,2], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Tx', [3,2], new_history, 1, ['DUKE', 'DUKE'], deck)
new_history = Pa.perform_action('As', [3,5], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ch', [0,5], new_history, 1, ['DUKE', 'DUKE'], deck)
new_history = Pa.perform_action('Sh', [0,5], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ld', [0,5], new_history, 1, ['DUKE', 'DUKE'], deck)

# Double Assassination: Challenging fake contessa
new_history = []
new_history = Pa.perform_action('In', [2,2], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Tx', [3,2], new_history, 1, ['DUKE', 'CAPTAIN'], deck)
new_history = Pa.perform_action('As', [3,5], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ba', [0,5], new_history, 1, ['DUKE', 'CAPTAIN'], deck)
new_history = Pa.perform_action('Ch', [0,5], new_history, 0, ['ASSASSIN', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ld', [0,5], new_history, 1, ['DUKE', 'CAPTAIN'], deck)

# Testing shuffling a challenged card
new_history = []
deck = ['ASSASSIN', 'ASSASSIN', 'ASSASSIN', 'CONTESSA', 'CONTESSA', 
        'CAPTAIN', 'CAPTAIN', 'CAPTAIN']
new_history = Pa.perform_action('Tx', [2,2], new_history, 0, ['DUKE', 'CONTESSA'], deck)
new_history = Pa.perform_action('Ch', [3,2], new_history, 1, ['DUKE', 'DUKE'], deck)

new_history = Pa.perform_action('Sh', [3,2], new_history, 0, ['DUKE', 'CONTESSA'], deck)

# Testing if the ratio of a new card is correct
cards = {}
for i in range(1_000_000):
    card = Pa.perform_action2('Sh', [3,2], ['Tx', 'Ch'], 0, ['DUKE', 'CONTESSA'], deck)[0]

    if card in cards:
        cards[card] += 1
    else:
        cards[card] = 1
print(cards)
"""



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



"""