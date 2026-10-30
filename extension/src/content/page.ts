// The page script's loop (plan E7; spec "When the panel appears"): it reads
// the board when the page opens and each time the page changes, and when a row
// has become finished it asks the background script for a hint and opens the
// panel with it. Typing, or anything else that leaves the finished rows as
// they were, changes nothing, so a closed panel stays closed until the next
// guess.
import type { HintRequest, HintResponse, Strategy } from '../messages';
import { shroud } from '../shroud';
import type { Panel, PanelView } from './panel';
import { NO_ROWS, readBoard, type Row } from './readBoard';
import { readHardMode } from './readHardMode';

export interface PageDeps {
  document: Document;
  storage: Pick<Storage, 'length' | 'key' | 'getItem'>;
  ask: (request: HintRequest) => Promise<HintResponse>;
  panel: Pick<Panel, 'show' | 'hide'>;
  strategy: Strategy;
  // How long to wait for NYT to draw the board before calling it unreadable.
  waitForBoardMs?: number;
}

function view(response: HintResponse, rows: Row[]): PanelView | undefined {
  if (response.kind === 'hint') return { kind: 'hint', shroud: shroud(response.word, rows) };
  if (response.kind === 'no-word') return { kind: 'no-word' };
  if (response.kind === 'error') return { kind: 'error', message: response.message };
  return undefined; // solved
}

// Starts watching; returns the function that stops it.
export function watchPage(deps: PageDeps): () => void {
  const { document, storage, ask, panel, strategy, waitForBoardMs = 5000 } = deps;
  let seen = ''; // the board the panel was last drawn for
  let latest = 0; // numbers each board, so an answer for an older one is dropped
  let waited = false;

  function draw(next: PanelView | undefined): void {
    if (next) panel.show(next);
    else panel.hide();
  }

  function check(): void {
    const board = readBoard(document);
    if (!board.readable && board.reason === NO_ROWS && !waited) return; // not drawn yet
    const key = board.readable ? JSON.stringify(board.rows) : board.reason;
    if (key === seen) return;
    seen = key;
    const ticket = ++latest;

    if (!board.readable) return draw({ kind: 'problem', reason: board.reason });
    const { rows } = board;
    const last = rows.at(-1);
    if (last?.pattern === 'GGGGG' || rows.length === 6) return draw(undefined); // game over
    const hardMode = readHardMode(storage, rows.length);
    if (!hardMode.readable) return draw({ kind: 'problem', reason: hardMode.reason });

    ask({ type: 'hint', rows, hardMode: hardMode.hardMode, strategy })
      .then(
        (response) => view(response, rows),
        (error: unknown): PanelView => ({ kind: 'error', message: String(error) }),
      )
      .then((next) => ticket === latest && draw(next));
  }

  const observer = new MutationObserver(check);
  observer.observe(document.body, {
    subtree: true,
    childList: true,
    attributeFilter: ['data-state', 'aria-label'],
  });
  const timer = setTimeout(() => {
    waited = true;
    check();
  }, waitForBoardMs);
  check();
  return () => {
    observer.disconnect();
    clearTimeout(timer);
  };
}
