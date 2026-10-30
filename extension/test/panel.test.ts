// @vitest-environment jsdom
// The panel drawn on NYT's page (plan E7, spec "What the panel shows").
import { beforeEach, describe, expect, test } from 'vitest';

import { createPanel, REPORT_URL } from '../src/content/panel';
import { shroud } from '../src/shroud';

const rows = [
  { word: 'olate', pattern: 'XXXXX' },
  { word: 'crony', pattern: 'XXYGX' },
];

beforeEach(() => {
  document.body.innerHTML = '<main id="game">NYT Wordle</main>';
});

// What a player sees inside the panel (its shadow root).
function view(panel: ReturnType<typeof createPanel>) {
  const root = panel.host.shadowRoot!;
  return {
    root,
    boxes: [...root.querySelectorAll('.box')].map((box) => box.textContent),
    label: root.querySelector('.boxes')?.getAttribute('aria-label'),
    text: root.querySelector('.body')!.textContent,
    // By its visible text or, for the × button, its accessible name.
    button: (name: string) =>
      [...root.querySelectorAll('button')].find(
        (button) => button.textContent === name || button.getAttribute('aria-label') === name,
      )!,
  };
}

describe('the hint', () => {
  test('shows the shroud: the green that fits, the rest covered', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'hint', shroud: shroud('bions', rows) });
    expect(view(panel).boxes).toEqual(['', '', '', 'n', '']);
    expect(view(panel).label).toBe('Hint: 4 of 5 letters covered');
    expect(panel.host.hidden).toBe(false);
  });

  test('"Reveal a letter" uncovers one more box per press', () => {
    const panel = createPanel(document, () => 0); // always the first covered box
    panel.show({ kind: 'hint', shroud: shroud('bions', rows) });
    view(panel).button('Reveal a letter').click();
    expect(view(panel).boxes).toEqual(['b', '', '', 'n', '']);
    view(panel).button('Reveal a letter').click();
    expect(view(panel).boxes).toEqual(['b', 'i', '', 'n', '']);
    expect(view(panel).label).toBe('Hint: 2 of 5 letters covered');
  });

  test('"Reveal the hint" uncovers every box, and then nothing is left to reveal', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'hint', shroud: shroud('bions', rows) });
    view(panel).button('Reveal the hint').click();
    expect(view(panel).boxes).toEqual(['b', 'i', 'o', 'n', 's']);
    expect(view(panel).label).toBe('Hint: bions');
    expect(view(panel).button('Reveal the hint').disabled).toBe(true);
    expect(view(panel).button('Reveal a letter').disabled).toBe(true);
  });

  test('a new hint starts covered again', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'hint', shroud: shroud('bions', rows) });
    view(panel).button('Reveal the hint').click();
    panel.show({ kind: 'hint', shroud: shroud('pownd', rows) });
    expect(view(panel).boxes).toEqual(['', '', '', 'n', '']);
  });
});

describe('the messages', () => {
  test('when no word fits', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'no-word' });
    expect(view(panel).text).toBe("we don't know what the word is :C");
    expect(view(panel).boxes).toEqual([]);
  });

  test("when nudge can't read the board: no hint, a report link, and the reason", () => {
    const panel = createPanel(document);
    panel.show({ kind: 'problem', reason: 'Row 2 has 4 tiles, not 5' });
    const { root, text, boxes } = view(panel);
    expect(boxes).toEqual([]);
    expect(text).toContain("nudge can't read the board");
    expect(text).toContain('Row 2 has 4 tiles, not 5');
    const link = root.querySelector('a')!;
    expect(link.href).toBe(REPORT_URL);
    expect(REPORT_URL).toBe('https://github.com/notpandiafox/nudge/issues');
    expect(link.rel).toContain('noopener');
  });

  test('when something else went wrong: its own heading, the report link, and the message', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'error', message: 'loading the solver: CompileError' });
    const { root, text } = view(panel);
    expect(text).toContain('nudge ran into a problem');
    expect(text).not.toContain("can't read the board");
    expect(text).toContain('loading the solver: CompileError');
    expect(root.querySelector('a')!.href).toBe(REPORT_URL);
  });

  test('page text in a reason stays text, never markup', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'problem', reason: '<img src=x onerror="alert(1)">' });
    expect(view(panel).root.querySelector('img')).toBeNull();
    expect(view(panel).text).toContain('<img src=x onerror="alert(1)">');
  });
});

describe('the panel on the page', () => {
  test('lives in one host element with its own shadow root, leaving the page alone', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'no-word' });
    panel.show({ kind: 'hint', shroud: shroud('bions', rows) });
    expect(document.querySelectorAll('nudge-panel')).toHaveLength(1);
    expect(panel.host.children).toHaveLength(0); // everything is inside the shadow root
    expect(view(panel).root.querySelector('style')).not.toBeNull();
    expect(document.querySelector('#game')!.textContent).toBe('NYT Wordle');
  });

  test('can be closed, and shows again on the next show()', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'no-word' });
    view(panel).button('Close').click();
    expect(panel.host.hidden).toBe(true);
    panel.show({ kind: 'no-word' });
    expect(panel.host.hidden).toBe(false);
  });

  test('stays hidden when hidden, despite its own style reset', () => {
    // :host { all: initial } outranks the browser's own [hidden] rule, so the
    // panel needs its own; jsdom doesn't compute styles, so check the rule.
    const style = createPanel(document).host.shadowRoot!.querySelector('style')!.textContent!;
    expect(style).toMatch(/:host\(\[hidden\]\)\s*\{\s*display:\s*none/);
  });

  test('the close button is a × named "Close"', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'no-word' });
    const close = view(panel).button('Close');
    expect(close.textContent).toBe('×');
  });

  test('hide() closes it', () => {
    const panel = createPanel(document);
    panel.show({ kind: 'no-word' });
    panel.hide();
    expect(panel.host.hidden).toBe(true);
  });
});
