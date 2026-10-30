// Light or dark, like Wordle (plan E9, E-D13; spec "How the panel looks"). Read
// from the page's colours, as the player sees them: not from a class name
// (they change with NYT's builds) and not from NYT's saved settings. That's
// right whichever way Wordle got dark: its own switch, or following Firefox.
export type Theme = 'light' | 'dark';

// A computed colour: rgb(r, g, b) or rgba(r, g, b, a), commas or spaces.
const RGB = /^rgba?\(([\d.]+)[\s,]+([\d.]+)[\s,]+([\d.]+)(?:[\s,/]+([\d.]+))?\)$/;

function parse(colour: string): { r: number; g: number; b: number; alpha: number } | undefined {
  const match = RGB.exec(colour.trim());
  if (!match) return undefined;
  const [, r, g, b, alpha = '1'] = match;
  return { r: Number(r), g: Number(g), b: Number(b), alpha: Number(alpha) };
}

// The colour the player sees behind the board: the first element, from the
// board's first row up (or from the page itself before the board is drawn),
// with a background colour that isn't fully see-through. Null when nothing
// has one, and the browser's own page colour shows.
export function backgroundBehindBoard(document: Document): string | null {
  let element: Element | null = document.querySelector('[aria-label^="Row "]') ?? document.body;
  for (; element; element = element.parentElement) {
    const colour = getComputedStyle(element).backgroundColor;
    if ((parse(colour)?.alpha ?? 0) > 0) return colour;
  }
  return null;
}

// Dark when the colour behind the board is less than half bright; Firefox's
// own setting (prefers-color-scheme) when there's no colour to go by.
export function readTheme(document: Document, systemDark: boolean): Theme {
  const colour = parse(backgroundBehindBoard(document) ?? '');
  if (!colour) return systemDark ? 'dark' : 'light';
  const brightness = 0.2126 * colour.r + 0.7152 * colour.g + 0.0722 * colour.b;
  return brightness < 128 ? 'dark' : 'light';
}

export interface ThemeDeps {
  document: Document;
  // matchMedia('(prefers-color-scheme: dark)'): Firefox's own setting.
  systemDark: {
    matches: boolean;
    addEventListener(type: 'change', listener: () => void): void;
    removeEventListener(type: 'change', listener: () => void): void;
  };
  panel: { setTheme(theme: Theme): void };
  save: (theme: Theme) => void;
}

// Sets the theme on the panel when the page opens, and again whenever it
// changes: Wordle's switch (a class or style on <html> or <body>), the board
// being drawn, or Firefox switching. Each change is saved once, for the
// toolbar's popup. Returns the function that stops watching.
export function watchTheme({ document, systemDark, panel, save }: ThemeDeps): () => void {
  let current: Theme | undefined;
  const check = () => {
    const theme = readTheme(document, systemDark.matches);
    if (theme === current) return;
    current = theme;
    panel.setTheme(theme);
    save(theme);
  };
  // Two observers: attributes only on <html> and <body>, so the board's own
  // class and style changes (its animations) don't wake this up.
  const root = new MutationObserver(check);
  root.observe(document.documentElement, { attributeFilter: ['class', 'style'] });
  root.observe(document.body, { attributeFilter: ['class', 'style'] });
  const drawn = new MutationObserver(check);
  drawn.observe(document.body, { childList: true, subtree: true });
  systemDark.addEventListener('change', check);
  check();
  return () => {
    root.disconnect();
    drawn.disconnect();
    systemDark.removeEventListener('change', check);
  };
}
