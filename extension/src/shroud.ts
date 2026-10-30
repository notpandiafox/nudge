// The shrouded hint (plan E6, spec "What the panel shows"): the hint word with
// its letters covered until the player reveals them. Pure, so the panel only
// draws what these return.
import type { Row } from './content/readBoard';

export interface Shroud {
  word: string;
  shown: boolean[]; // one per letter: is that box uncovered
}

// A box starts uncovered only where a finished row has a green and the hint
// has that same letter: the hint is sometimes a probe word that doesn't keep
// the greens, and then they don't show (in hard mode they always fit).
export function shroud(hint: string, rows: Row[]): Shroud {
  const shown = [...hint].map((letter, i) =>
    rows.some((row) => row.pattern[i] === 'G' && row.word[i] === letter),
  );
  return { word: hint, shown };
}

// Uncovers one covered box, chosen at random; does nothing once all show.
export function revealLetter(current: Shroud, random: () => number = Math.random): Shroud {
  const covered = current.shown.flatMap((isShown, i) => (isShown ? [] : [i]));
  if (covered.length === 0) return current;
  const pick = covered[Math.floor(random() * covered.length)];
  return { word: current.word, shown: current.shown.map((isShown, i) => isShown || i === pick) };
}

export function revealAll(current: Shroud): Shroud {
  return { word: current.word, shown: current.shown.map(() => true) };
}
