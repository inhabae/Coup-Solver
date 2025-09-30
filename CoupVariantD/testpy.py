from itertools import combinations
from math import comb

def calculate_pair_probabilities(removed_cards):
    # Initial card counts: 3 cards per kind (A, B, C, D, E)
    kinds = ['A', 'B', 'C', 'D', 'E']
    card_counts = {'A': 3, 'B': 3, 'C': 3, 'D': 3, 'E': 3}
    
    # Update counts based on removed cards
    for card in removed_cards:
        if card in card_counts:
            card_counts[card] -= 1
            if card_counts[card] < 0:
                raise ValueError(f"Cannot remove more {card} cards than available")
    
    # Total cards left
    total_cards = sum(card_counts.values())
    if total_cards != 13:
        raise ValueError("Expected 13 cards remaining after removing 2")
    
    # Total unordered pairs: C(13, 2)
    total_pairs = comb(total_cards, 2)  # Should be 78
    
    # Calculate probabilities for each pair
    probabilities = {}
    
    # Same-kind pairs (e.g., A,A)
    for kind in kinds:
        pair = f"({kind},{kind})"
        if card_counts[kind] >= 2:
            favorable = comb(card_counts[kind], 2)
            probabilities[pair] = favorable / total_pairs
        else:
            probabilities[pair] = 0.0
    
    # Different-kind pairs (e.g., A,B)
    for kind1, kind2 in combinations(kinds, 2):
        pair = f"({kind1},{kind2})"
        if card_counts[kind1] >= 1 and card_counts[kind2] >= 1:
            favorable = card_counts[kind1] * card_counts[kind2]
            probabilities[pair] = favorable / total_pairs
        else:
            probabilities[pair] = 0.0
    
    return probabilities

# Example usage
def print_probabilities(probabilities):
    print("Probabilities for each unordered pair:")
    for pair, prob in sorted(probabilities.items()):
        print(f"{pair}: {prob:.6f} ({prob * 100:.2f}%)")

# Example 1: Remove two cards of the same kind (e.g., two A's)
removed = ['A', 'A']
print("Example 1: Removed cards", removed)
probs = calculate_pair_probabilities(removed)
print_probabilities(probs)

# Example 2: Remove two cards of different kinds (e.g., one A and one B)
print("\nExample 2: Removed cards ['A', 'B']")
probs = calculate_pair_probabilities(['A', 'B'])
print_probabilities(probs)