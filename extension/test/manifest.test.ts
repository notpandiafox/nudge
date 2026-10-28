// The manifest is where the extension's privacy promises live (spec,
// "Interfaces"): it may touch only the Wordle page, store only its own two
// settings, and compile WebAssembly without allowing eval.
import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';

const manifest = JSON.parse(readFileSync(new URL('../manifest.json', import.meta.url), 'utf8'));
const packageJson = JSON.parse(readFileSync(new URL('../package.json', import.meta.url), 'utf8'));

const WORDLE = 'https://www.nytimes.com/games/wordle/*';

describe('manifest', () => {
  // 142: the first Firefox (desktop 140, Android 142) that knows the "collects no
  // data" declaration below; still well behind the current ESR (153).
  test('is Manifest V3 for Firefox 142 and later', () => {
    expect(manifest.manifest_version).toBe(3);
    expect(manifest.browser_specific_settings.gecko.strict_min_version).toBe('142.0');
  });

  test('touches only the Wordle page', () => {
    expect(manifest.host_permissions).toEqual([WORDLE]);
    expect(manifest.content_scripts.map((script: { matches: string[] }) => script.matches)).toEqual(
      [[WORDLE]],
    );
  });

  test('asks for no permission but its own storage', () => {
    expect(manifest.permissions).toEqual(['storage']);
    expect(manifest.optional_permissions).toBeUndefined();
  });

  test('collects no data', () => {
    expect(manifest.browser_specific_settings.gecko.data_collection_permissions).toEqual({
      required: ['none'],
    });
  });

  test('may compile WebAssembly but never runs generated code', () => {
    const policy: string = manifest.content_security_policy.extension_pages;
    expect(policy).toContain("'wasm-unsafe-eval'");
    expect(policy).not.toMatch(/'unsafe-eval'|'unsafe-inline'|https?:/);
  });

  test("has the package's version, so a release tag can be checked against one number", () => {
    expect(manifest.version).toBe(packageJson.version);
  });
});
