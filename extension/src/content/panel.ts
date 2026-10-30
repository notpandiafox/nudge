// nudge's panel, drawn on NYT's page (plan E7, E-D1, E-D8; spec "What the
// panel shows"). It lives in a shadow root, so NYT's styles can't change it and
// its styles can't change NYT's page. It only draws: the page script decides
// what to show. Text is always set as text, never as markup.
import { revealAll, revealLetter, type Shroud } from '../shroud';

export const REPORT_URL = 'https://github.com/notpandiafox/nudge/issues';

export type PanelView =
  | { kind: 'hint'; shroud: Shroud }
  | { kind: 'no-word' }
  // The board or hard mode can't be read.
  | { kind: 'problem'; reason: string }
  // Something else went wrong: the solver, or reaching the background script.
  | { kind: 'error'; message: string };

export interface Panel {
  host: HTMLElement;
  show(view: PanelView): void;
  hide(): void;
}

const STYLE = `
  :host { all: initial; }
  /* all: initial outranks the browser's own [hidden] rule, so say it again. */
  :host([hidden]) { display: none; }
  .panel {
    position: fixed; top: 64px; right: 16px; z-index: 2147483647; width: 260px;
    padding: 12px 14px; border-radius: 12px; background: #ffffff; color: #1a1a1b;
    box-shadow: 0 4px 20px rgb(0 0 0 / 25%); font: 14px/1.4 system-ui, sans-serif;
  }
  header { display: flex; justify-content: space-between; align-items: center; font-weight: 700; }
  .close { border: 0; background: none; font-size: 20px; cursor: pointer; color: inherit; }
  .boxes { display: flex; gap: 6px; margin: 10px 0; }
  .box {
    width: 36px; height: 36px; display: grid; place-items: center; border-radius: 4px;
    font-size: 20px; font-weight: 700; text-transform: uppercase;
    background: #787c7e; color: #ffffff;
  }
  .box.shown { background: #6aaa64; }
  .actions { display: flex; gap: 6px; }
  .actions button {
    flex: 1; padding: 6px; border: 1px solid #d3d6da; border-radius: 6px; background: #f6f7f8;
    font: inherit; cursor: pointer; color: inherit;
  }
  .actions button:disabled { opacity: 0.5; cursor: default; }
  .reason { font-size: 12px; color: #6b6f73; }
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

export function createPanel(document: Document, random: () => number = Math.random): Panel {
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
  panel.append(header, body);
  root.append(style, panel);

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

  return {
    host,
    show(view) {
      if (!host.isConnected) document.body.append(host);
      if (view.kind === 'hint') drawHint(view.shroud);
      else if (view.kind === 'no-word') {
        body.replaceChildren(element(document, 'p', "we don't know what the word is :C"));
      } else if (view.kind === 'problem') drawProblem("nudge can't read the board.", view.reason);
      else drawProblem('nudge ran into a problem.', view.message);
      host.hidden = false;
    },
    hide() {
      host.hidden = true;
    },
  };
}
