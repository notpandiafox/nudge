// The page script's loop (plan E7, E8; spec "When the panel appears"): it
// reads the board when the page opens and each time the page changes, and when
// a row has become finished it asks the background script for a hint and draws
// it in the panel, opening the panel when "open after each guess" is on.
// Typing, or anything else that leaves the finished rows as they were, changes
// nothing, so a closed panel stays closed until the next guess.
import type { HintRequest, HintResponse } from '../messages';
import type { Settings } from '../settings';
import { shroud } from '../shroud';
import { PANEL_SPACE, type Panel, type PanelView } from './panel';
import { NO_ROWS, readBoard, ROWS, type Row } from './readBoard';
import { readHardMode } from './readHardMode';

export interface PageDeps {
  document: Document;
  storage: Pick<Storage, 'length' | 'key' | 'getItem'>;
  ask: (request: HintRequest) => Promise<HintResponse>;
  panel: Pick<Panel, 'draw' | 'show' | 'hide' | 'setSettings'>;
  settings: Settings; // as saved when the page opened
  // How long to wait for NYT to save a new row before calling hard mode
  // unreadable: it saves a moment after the tiles turn.
  hardModeWaitMs?: number;
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
  const { document, storage, ask, panel, hardModeWaitMs = 2000 } = deps;
  let settings = deps.settings;
  let seen = ''; // the board the panel was last drawn for
  let latest = 0; // numbers each board, so an answer for an older one is dropped
  let retry: ReturnType<typeof setTimeout> | undefined;

  // The guess that ends the game closes the panel; any other guess opens it,
  // if the player wants that. A switch flipped changes only what it shows.
  // No board (NYT's welcome screen, until Play) waits for the button, and so
  // does a window too narrow for the panel beside the board.
  function draw(next: PanelView, afterGuess: boolean): void {
    if (next.kind === 'over') {
      panel.draw(next);
      if (afterGuess) panel.hide();
    } else if (afterGuess && settings.autoPanel && next.kind !== 'no-board' && besideBoard())
      panel.show(next);
    else panel.draw(next);
  }

  function besideBoard(): boolean {
    const right = document.querySelector(ROWS)?.getBoundingClientRect().right ?? 0;
    return right <= window.innerWidth - PANEL_SPACE;
  }

  function check(afterGuess: boolean): void {
    const board = readBoard(document);
    const key = board.readable ? JSON.stringify(board.rows) : board.reason;
    if (key === seen) return;
    seen = key;
    const ticket = ++latest;

    if (!board.readable) {
      const next: PanelView =
        board.reason === NO_ROWS ? { kind: 'no-board' } : { kind: 'problem', reason: board.reason };
      return draw(next, afterGuess);
    }
    const { rows } = board;
    const last = rows.at(-1);
    if (last?.pattern === 'GGGGG' || rows.length === 6) return draw({ kind: 'over' }, afterGuess);
    hint(rows, ticket, afterGuess, Date.now());
  }

  function hint(rows: Row[], ticket: number, afterGuess: boolean, since: number): void {
    if (ticket !== latest) return; // a newer board took over
    const hardMode = readHardMode(storage, rows.length);
    if (!hardMode.readable) {
      if (Date.now() - since < hardModeWaitMs) {
        retry = setTimeout(() => hint(rows, ticket, afterGuess, since), 50);
        return;
      }
      return draw({ kind: 'problem', reason: hardMode.reason }, afterGuess);
    }

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
  check(true);

  return {
    stop() {
      observer.disconnect();
      clearTimeout(retry);
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
