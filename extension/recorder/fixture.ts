// Turns the open Wordle page into a saved test copy (plan E2). What this keeps
// is what gets committed, so it keeps by allowlist: unknown data is dropped,
// never copied.

// Stand-ins for real NYT account numbers, in order of first appearance.
export const FAKE_ACCOUNTS = ['10000001', '10000002', '10000003'] as const;

// The two saved entries the hard-mode reader needs (spike §3); nothing else
// in NYT's saved data is recorded.
const RECORDED_ENTRY = /^games-(state|settings)-wordleV2\/(.+)$/;
// Inside them, only these facts: whether hard mode is on, and which game it is.
const KEPT_FACTS = new Set(['hardMode', 'currentRowIndex', 'status', 'puzzleId', 'schemaVersion']);

type Json = string | number | boolean | null | Json[] | { [key: string]: Json };
type JsonObject = { [key: string]: Json };
type SavedData = Pick<Storage, 'length' | 'key' | 'getItem'>;

function isObject(value: Json): value is JsonObject {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

// Keeps objects only for the kept facts inside them; drops plain lists (such
// as the guesses) and every other value.
function keepFacts(object: JsonObject): JsonObject {
  const kept: JsonObject = {};
  for (const [key, value] of Object.entries(object)) {
    if (isObject(value)) {
      const inner = keepFacts(value);
      if (Object.keys(inner).length > 0) kept[key] = inner;
    } else if (Array.isArray(value)) {
      const items = value.filter(isObject).map(keepFacts);
      if (items.some((item) => Object.keys(item).length > 0)) kept[key] = items;
    } else if (KEPT_FACTS.has(key)) {
      kept[key] = value;
    }
  }
  return kept;
}

function parse(text: string): Json | undefined {
  try {
    return JSON.parse(text) as Json;
  } catch {
    return undefined;
  }
}

export function recordStorage(storage: SavedData): Record<string, JsonObject> {
  const recorded: Record<string, JsonObject> = {};
  const fakeFor = new Map<string, string>();
  for (let i = 0; i < storage.length; ++i) {
    const key = storage.key(i) ?? '';
    const match = RECORDED_ENTRY.exec(key);
    if (!match) continue;
    const [, kind, account = ''] = match;
    if (account !== 'ANON' && !fakeFor.has(account)) {
      fakeFor.set(account, FAKE_ACCOUNTS[fakeFor.size] ?? `1000000${fakeFor.size + 1}`);
    }
    const value = parse(storage.getItem(key) ?? '');
    const name = `games-${kind}-wordleV2/${account === 'ANON' ? 'ANON' : fakeFor.get(account)}`;
    recorded[name] =
      value !== undefined && isObject(value) ? keepFacts(value) : { unreadable: true };
  }
  return recorded;
}

// The rows and their tiles, and nothing around them: not even their
// container, which on an unusual page could be the whole page (where names,
// menus and ads live).
export function recordBoard(document: Document): string | null {
  const rows = [...document.querySelectorAll('[aria-label^="Row "]')];
  return rows.length > 0 ? rows.map((row) => row.outerHTML).join('\n') : null;
}

export interface Fixture {
  recordedOn: string;
  note: string;
  page: string;
  boardHtml: string | null;
  storage: Record<string, JsonObject>;
}

export function makeFixture(input: {
  document: Document;
  storage: SavedData;
  href: string;
  note: string;
  now: Date;
}): Fixture {
  const url = new URL(input.href);
  return {
    recordedOn: input.now.toISOString().slice(0, 10),
    note: input.note,
    page: `${url.origin}${url.pathname}`,
    boardHtml: recordBoard(input.document),
    storage: recordStorage(input.storage),
  };
}
