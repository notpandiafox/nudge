// The game's rules written again for the tests, so the solver never grades
// itself (the same idea as the solver's own reference checkers).

// Greens first, then Yellows left to right, each using up a copy.
export function feedback(guess: string, answer: string): string {
  const result = ['X', 'X', 'X', 'X', 'X'];
  const left = [...answer];
  for (let i = 0; i < 5; ++i) {
    if (guess[i] === answer[i]) {
      result[i] = 'G';
      left[i] = '#';
    }
  }
  for (let i = 0; i < 5; ++i) {
    const found = left.indexOf(guess[i]!);
    if (result[i] !== 'G' && found !== -1) {
      result[i] = 'Y';
      left[found] = '#';
    }
  }
  return result.join('');
}

// Hard mode's rule B: every Green stays, and every lit letter of one earlier
// guess needs its own copy in the new guess.
export function hardModeAllows(rows: { word: string; pattern: string }[], guess: string): boolean {
  return rows.every(({ word, pattern }) => {
    const left = [...guess];
    for (let i = 0; i < 5; ++i) {
      if (pattern[i] === 'G' && guess[i] !== word[i]) return false;
      if (pattern[i] !== 'X') {
        const found = left.indexOf(word[i]!);
        if (found === -1) return false;
        left[found] = '#';
      }
    }
    return true;
  });
}
