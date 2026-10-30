// What the page script and the background script send each other (plan §2).
import type { Row } from './content/readBoard';

export type Strategy = 'minimax' | 'entropy';

// The finished rows on screen, the player's hard-mode setting, and the strategy.
export interface HintRequest {
  type: 'hint';
  rows: Row[];
  hardMode: boolean;
  strategy: Strategy;
}

export type HintResponse =
  | { kind: 'hint'; word: string }
  // The board already has the answer: nothing to hint.
  | { kind: 'solved'; word: string }
  // No word fits: the answer isn't in the lists (fallback) or the board is
  // impossible. The panel says "we don't know what the word is :C" (spec).
  | { kind: 'no-word' }
  // Something the reader should never send; a bug, not a game state.
  | { kind: 'error'; message: string };

// The page script, on start: "this tab is Wordle", so the toolbar button
// toggles the panel there instead of opening the "Nudge" popup (plan E8).
export interface WordlePage {
  type: 'wordle-page';
}

// The background script to that tab's page script: the button was clicked.
export interface Toggle {
  type: 'toggle';
}
