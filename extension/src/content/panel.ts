// nudge's panel, drawn on NYT's page (plan E7, E-D1, E-D8; spec "What the
// panel shows"). It lives in a shadow root, so NYT's styles can't change it and
// its styles can't change NYT's page. It only draws: the page script decides
// what to show. Text is always set as text, never as markup.
import type { Settings } from '../settings';
import { revealAll, revealLetter, type Shroud } from '../shroud';
import type { Theme } from './theme';

export const REPORT_URL = 'https://github.com/notpandiafox/nudge/issues';

export type PanelView =
  | { kind: 'hint'; shroud: Shroud }
  | { kind: 'no-word' }
  // The board or hard mode can't be read.
  | { kind: 'problem'; reason: string }
  // Something else went wrong: the solver, or reaching the background script.
  | { kind: 'error'; message: string }
  // The game is won or lost (shown only when the player opens the panel).
  | { kind: 'over' };

export interface Panel {
  host: HTMLElement;
  draw(view: PanelView): void; // changes what it shows, open or not
  show(view: PanelView): void; // draws, and opens
  hide(): void;
  toggle(): void; // the toolbar button
  setSettings(settings: Settings): void;
  setTheme(theme: Theme): void; // light or dark, like Wordle (plan E9)
}

export interface PanelOptions {
  random?: () => number; // picks the letter "Reveal a letter" uncovers
  onSettings?: (change: Partial<Settings>) => void; // a switch was flipped
}

// Wordle's own colours, light and dark (spec "How the panel looks").
// all: initial leaves these custom properties alone.
const STYLE = `
  :host {
    all: initial; color-scheme: light;
    --page: #ffffff; --text: #1a1a1b; --line: #d3d6da; --covered: #787c7e; --green: #6aaa64;
    --button: #f6f7f8; --quiet: #6b6f73; --link: #0b57d0; --shadow: rgb(0 0 0 / 25%);
  }
  :host([theme="dark"]) {
    color-scheme: dark;
    --page: #121213; --text: #f8f8f8; --line: #3a3a3c; --covered: #3a3a3c; --green: #538d4e;
    --button: #2a2a2c; --quiet: #a0a3a7; --link: #8ab4f8; --shadow: rgb(0 0 0 / 60%);
  }
  /* all: initial outranks the browser's own [hidden] rule, so say it again. */
  :host([hidden]) { display: none; }
  .panel {
    position: fixed; top: 64px; right: 16px; z-index: 2147483647; width: 260px;
    padding: 12px 14px; border-radius: 12px; background: var(--page); color: var(--text);
    border: 1px solid var(--line); box-shadow: 0 4px 20px var(--shadow);
    font: 14px/1.4 system-ui, sans-serif;
  }
  a { color: var(--link); }
  header { display: flex; justify-content: space-between; align-items: center; font-weight: 700; }
  .close { border: 0; background: none; font-size: 20px; cursor: pointer; color: inherit; }
  .boxes { display: flex; gap: 6px; margin: 10px 0; }
  .box {
    width: 36px; height: 36px; display: grid; place-items: center; border-radius: 4px;
    font-size: 20px; font-weight: 700; text-transform: uppercase;
    background: var(--covered); color: #ffffff;
  }
  .box.shown { background: var(--green); }
  .actions { display: flex; gap: 6px; }
  .actions button {
    flex: 1; padding: 6px; border: 1px solid var(--line); border-radius: 6px; background: var(--button);
    font: inherit; cursor: pointer; color: inherit;
  }
  .actions button:disabled { opacity: 0.5; cursor: default; }
  .reason { font-size: 12px; color: var(--quiet); }
  .switches {
    display: flex; flex-wrap: wrap; gap: 4px 12px; margin-top: 10px; padding-top: 8px;
    border-top: 1px solid var(--line); font-size: 12px;
  }
  .switches label { display: flex; align-items: center; gap: 4px; cursor: pointer; }
`;

function element<K extends keyof HTMLElementTagNameMap>(
  document: Document,
  tag: K,
  text = '',
  className = '',
): HTMLElementTagNameMap[K] {
  const made = document.createElement(tag);
  made.textContent = text;
  if (className) made.className = className;
  return made;
}

// A checkbox or radio button inside its label, named by that label.
function choice(document: Document, type: string, name: string, onChange: () => void) {
  const input = element(document, 'input');
  input.type = type;
  input.name = type === 'radio' ? 'strategy' : name;
  input.setAttribute('aria-label', name);
  input.addEventListener('change', onChange);
  const label = element(document, 'label');
  label.append(input, name);
  return { input, label };
}

export function createPanel(document: Document, options: PanelOptions = {}): Panel {
  const { random = Math.random, onSettings } = options;
  const host = document.createElement('nudge-panel');
  host.hidden = true;
  const root = host.attachShadow({ mode: 'open' });
  const style = element(document, 'style', STYLE);
  const panel = element(document, 'section', '', 'panel');
  panel.setAttribute('aria-label', 'nudge');
  const header = element(document, 'header', 'nudge');
  const close = element(document, 'button', '×', 'close');
  close.setAttribute('aria-label', 'Close');
  close.addEventListener('click', () => (host.hidden = true));
  header.append(close);
  const body = element(document, 'div', '', 'body');
  // The two switches (spec "Switches"); flipping one passes on just that change.
  const auto = choice(document, 'checkbox', 'Open after each guess', () =>
    onSettings?.({ autoPanel: auto.input.checked }),
  );
  auto.input.setAttribute('role', 'switch');
  const minimax = choice(document, 'radio', 'Minimax', () => onSettings?.({ strategy: 'minimax' }));
  const entropy = choice(document, 'radio', 'Entropy', () => onSettings?.({ strategy: 'entropy' }));
  const switches = element(document, 'div', '', 'switches');
  switches.append(auto.label, minimax.label, entropy.label);
  panel.append(header, body, switches);
  root.append(style, panel);

  function open(): void {
    if (!host.isConnected) document.body.append(host);
    host.hidden = false;
  }

  // Draws the shroud and its two buttons; the buttons draw it again revealed.
  function drawHint(current: Shroud): void {
    const covered = current.shown.filter((isShown) => !isShown).length;
    const boxes = element(document, 'div', '', 'boxes');
    boxes.setAttribute('role', 'img');
    boxes.setAttribute(
      'aria-label',
      covered === 0 ? `Hint: ${current.word}` : `Hint: ${covered} of 5 letters covered`,
    );
    current.shown.forEach((isShown, i) => {
      boxes.append(
        element(document, 'span', isShown ? current.word[i] : '', isShown ? 'box shown' : 'box'),
      );
    });
    const all = element(document, 'button', 'Reveal the hint');
    const one = element(document, 'button', 'Reveal a letter');
    all.disabled = one.disabled = covered === 0;
    all.addEventListener('click', () => drawHint(revealAll(current)));
    one.addEventListener('click', () => drawHint(revealLetter(current, random)));
    const actions = element(document, 'div', '', 'actions');
    actions.append(all, one);
    body.replaceChildren(boxes, actions);
  }

  function drawProblem(heading: string, reason: string): void {
    const link = element(document, 'a', 'report it on GitHub');
    link.href = REPORT_URL;
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
    const ask = element(document, 'p', 'Please ');
    ask.append(link, '.');
    body.replaceChildren(
      element(document, 'p', heading),
      ask,
      element(document, 'p', reason, 'reason'),
    );
  }

  function draw(view: PanelView): void {
    if (view.kind === 'hint') drawHint(view.shroud);
    else if (view.kind === 'no-word') {
      body.replaceChildren(element(document, 'p', "we don't know what the word is :C"));
    } else if (view.kind === 'over')
      body.replaceChildren(element(document, 'p', 'This game is over.'));
    else if (view.kind === 'problem') drawProblem("nudge can't read the board.", view.reason);
    else drawProblem('nudge ran into a problem.', view.message);
  }

  return {
    host,
    draw,
    show(view) {
      draw(view);
      open();
    },
    hide() {
      host.hidden = true;
    },
    toggle() {
      if (host.hidden) open();
      else host.hidden = true;
    },
    setTheme(theme) {
      host.setAttribute('theme', theme);
    },
    setSettings(settings) {
      auto.input.checked = settings.autoPanel;
      minimax.input.checked = settings.strategy === 'minimax';
      entropy.input.checked = settings.strategy === 'entropy';
    },
  };
}
