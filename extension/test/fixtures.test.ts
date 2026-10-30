// Every committed copy of NYT's page passes this before it can be used (plan
// E2). Written separately from the recorder on purpose, so a bug in the
// recorder can't also hide here.
import { readdirSync, readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';

const folder = new URL('./fixtures/', import.meta.url);
const files = readdirSync(folder).filter((name) => name.endsWith('.json'));

const ENTRY = /^games-(state|settings)-wordleV2\/(ANON|1000000[1-9])$/;
const FACTS = new Set(['hardMode', 'currentRowIndex', 'status', 'puzzleId', 'schemaVersion']);
const CONTAINERS = new Set(['states', 'data']);

// Every key in the saved data is a known container or an allowed fact.
function unexpectedKeys(value: unknown, path = ''): string[] {
  if (Array.isArray(value))
    return value.flatMap((item, i) => unexpectedKeys(item, `${path}[${i}]`));
  if (typeof value !== 'object' || value === null) return [];
  return Object.entries(value).flatMap(([key, inner]) =>
    CONTAINERS.has(key) || FACTS.has(key)
      ? unexpectedKeys(inner, `${path}.${key}`)
      : [`${path}.${key}`],
  );
}

describe('saved copies of NYT’s page', () => {
  test('exist', () => {
    expect(files.length).toBeGreaterThan(0);
  });

  describe.each(files)('%s', (file) => {
    const text = readFileSync(new URL(file, folder), 'utf8');
    const fixture = JSON.parse(text);

    test('has a note, a date and the board', () => {
      expect(fixture.note).toEqual(expect.any(String));
      expect(fixture.recordedOn).toMatch(/^\d{4}-\d{2}-\d{2}$/);
      expect(fixture.boardHtml).toContain('aria-label="Row 1"');
    });

    test('keeps only the two Wordle entries, with fake account numbers', () => {
      for (const key of Object.keys(fixture.storage)) expect(key).toMatch(ENTRY);
    });

    test('keeps no saved data beyond the hard-mode facts', () => {
      // The entry names are checked above; this looks inside each entry.
      const entries = Object.entries(fixture.storage);
      expect(entries.flatMap(([name, entry]) => unexpectedKeys(entry, name))).toEqual([]);
    });

    test('holds no account number or email address anywhere', () => {
      const numbers = text.match(/\d{6,}/g) ?? [];
      expect(numbers.filter((n) => !/^1000000[1-9]$/.test(n))).toEqual([]);
      expect(text).not.toMatch(/[\w.+-]+@[\w-]+\.[\w.]+/);
    });
  });
});
