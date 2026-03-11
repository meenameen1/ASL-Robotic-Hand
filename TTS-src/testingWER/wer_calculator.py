#!/usr/bin/env python3
"""Calculate Word Error Rate (WER) between two text files."""

from difflib import SequenceMatcher

def extract_words(text):
    """Extract words from text and convert to lowercase."""
    return text.lower().split()

def calculate_wer(reference, hypothesis):
    """
    Calculate Word Error Rate using dynamic programming (edit distance).
    
    WER = (S + D + I) / N * 100
    where:
    - S = substitutions
    - D = deletions
    - I = insertions
    - N = number of words in reference
    """
    ref_words = extract_words(reference)
    hyp_words = extract_words(hypothesis)
    
    n = len(ref_words)
    m = len(hyp_words)
    
    # Create DP table
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    
    # Initialize first column (deletions)
    for i in range(n + 1):
        dp[i][0] = i
    
    # Initialize first row (insertions)
    for j in range(m + 1):
        dp[0][j] = j
    
    # Fill the DP table
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            if ref_words[i-1] == hyp_words[j-1]:
                dp[i][j] = dp[i-1][j-1]
            else:
                # Substitution, deletion, or insertion
                dp[i][j] = 1 + min(
                    dp[i-1][j-1],  # substitution
                    dp[i-1][j],    # deletion
                    dp[i][j-1]     # insertion
                )
    
    # Backtrack to find operations
    i, j = n, m
    substitutions = 0
    deletions = 0
    insertions = 0
    
    while i > 0 or j > 0:
        if i > 0 and j > 0 and ref_words[i-1] == hyp_words[j-1]:
            i -= 1
            j -= 1
        elif i > 0 and j > 0 and dp[i][j] == dp[i-1][j-1] + 1:
            substitutions += 1
            i -= 1
            j -= 1
        elif i > 0 and dp[i][j] == dp[i-1][j] + 1:
            deletions += 1
            i -= 1
        elif j > 0 and dp[i][j] == dp[i][j-1] + 1:
            insertions += 1
            j -= 1
        else:
            break
    
    wer = (substitutions + deletions + insertions) / n * 100 if n > 0 else 0
    
    return {
        'wer': wer,
        'substitutions': substitutions,
        'deletions': deletions,
        'insertions': insertions,
        'ref_length': n,
        'hyp_length': m,
        'distance': dp[n][m]
    }

def read_words_file(filename):
    """Read the words.txt file which has numbered entries."""
    words = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                # Remove the number prefix (e.g., "1. The" -> "The")
                parts = line.split('. ', 1)
                if len(parts) == 2:
                    words.append(parts[1])
    return ' '.join(words)

def read_result_file(filename):
    """Read the result.txt file which contains continuous text."""
    with open(filename, 'r') as f:
        return f.read().strip()

if __name__ == '__main__':
    # Read files
    reference = read_words_file('words.txt')
    hypothesis = read_result_file('result.txt')
    
    print("Reference text:")
    print(f"  {reference[:100]}..." if len(reference) > 100 else f"  {reference}")
    print()
    print("Hypothesis text:")
    print(f"  {hypothesis[:100]}..." if len(hypothesis) > 100 else f"  {hypothesis}")
    print()
    
    # Calculate WER
    results = calculate_wer(reference, hypothesis)
    
    print("=" * 50)
    print("WER Calculation Results")
    print("=" * 50)
    print(f"Word Error Rate: {results['wer']:.2f}%")
    print(f"Reference Length: {results['ref_length']} words")
    print(f"Hypothesis Length: {results['hyp_length']} words")
    print(f"Substitutions: {results['substitutions']}")
    print(f"Deletions: {results['deletions']}")
    print(f"Insertions: {results['insertions']}")
    print(f"Edit Distance: {results['distance']}")
