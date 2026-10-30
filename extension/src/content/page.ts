// The page script's loop (plan E7, E8; spec "When the panel appears"): it
// reads the board when the page opens and each time the page changes, and when
// a row has become finished it asks the background script for a hint and draws
// it in the panel, opening the panel when "open after each guess" is on.
// Typing, or anything else that leaves the finished rows as they were, changes
// nothing, so a closed panel stays closed until the next guess.
import type { HintRequest, HintResponse } from '../messages';
import type { Settings } from '../settings';
import { shroud } from '../shroud';
import type { Panel, PanelView } from './panel';
import { NO_ROWS, readBoard, type Row } from './readBoard';
import { readHardMode } from './readHardMode';

export interface PageDeps {
  document: Document;
  storage: Pick<Storage, 'length' | 'key' | 'getItem'>;
  ask: (request: HintRequest) => Promise<HintResponse>;
  panel: Pick<Panel, 'draw' | 'show' | 'hide' | 'setSettings'>;
  settings: Settings; // as saved when the page opened
  // How long to wait for NYT to draw the board before calling it unreadable.
  waitForBoardMs?: number;
}

export interface Page {
  stop(): void;
  // The switches changed (in this panel, or in another tab's).
  setSettings(settings: Settings): void;
}

function view(response: HintResponse, rows: Row[]): PanelView {
  if (response.kind === 'hint') return { kind: 'hint', shroud: shroud(response.word, rows) };
  if (response.kind === 'no-word') return { kind: 'no-word' };
  if (response.kind === 'error') return { kind: 'error', message: response.message };
  return { kind: 'over' }; // solved
}

export function watchPage(deps: PageDeps): Page {
  const { document, storage, ask, panel, waitForBoardMs = 5000 } = deps;
  let settings = deps.settings;
  let seen = ''; // the board the panel was last drawn for
  let latest = 0; // numbers each board, so an answer for an older one is dropped
  let waited = false;

  // The guess that ends the game closes the panel; any other guess opens it,
  // if the player wants that. A switch flipped changes only what it shows.
  function draw(next: PanelView, afterGuess: boolean): void {
    if (next.kind === 'over') {
      panel.draw(next);
      if (afterGuess) panel.hide();
    } else if (afterGuess && settings.autoPanel) panel.show(next);
    else panel.draw(next);
  }

  function check(afterGuess: boolean): void {
    const board = readBoard(document);
    if (!board.readable && board.reason === NO_ROWS && !waited) return; // not drawn yet
    const key = board.readable ? JSON.stringify(board.rows) : board.reason;
    if (key === seen) return;
    seen = key;
    const ticket = ++latest;

    if (!board.readable) return draw({ kind: 'problem', reason: board.reason }, afterGuess);
    const { rows } = board;
    const last = rows.at(-1);
    if (last?.pattern === 'GGGGG' || rows.length === 6) return draw({ kind: 'over' }, afterGuess);
    const hardMode = readHardMode(storage, rows.length);
    if (!hardMode.readable) return draw({ kind: 'problem', reason: hardMode.reason }, afterGuess);

    const { strategy } = settings;
    ask({ type: 'hint', rows, hardMode: hardMode.hardMode, strategy })
      .then(
        (response) => view(response, rows),
        (error: unknown): PanelView => ({ kind: 'error', message: String(error) }),
      )
      .then((next) => ticket === latest && draw(next, afterGuess));
  }

  panel.setSettings(settings);
  const observer = new MutationObserver(() => check(true));
  observer.observe(document.body, {
    subtree: true,
    childList: true,
    attributeFilter: ['data-state', 'aria-label'],
  });
  const timer = setTimeout(() => {
    waited = true;
    check(true);
  }, waitForBoardMs);
  check(true);

  return {
    stop() {
      observer.disconnect();
      clearTimeout(timer);
    },
    setSettings(next) {
      const newStrategy = next.strategy !== settings.strategy;
      settings = next;
      panel.setSettings(next);
      if (newStrategy) {
        seen = ''; // the same board, asked again: a new hint at once
        check(false);
      }
    },
  };
}
