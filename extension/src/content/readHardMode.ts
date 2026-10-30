// Reads the player's hard-mode setting from NYT's saved data (spec, "Reading
// hard mode"; spike §3). It reads only the hardMode flag, plus the number of
// played rows to pick the right game: never the guesses or the answer.
//
// NYT can keep several entries at once (one per account, plus ANON for
// signed out), and an account's entry can hold an old game. So: today's game
// is the one whose played rows match the board on screen; failing that, the
// settings; and when those disagree, it says it can't tell rather than guess.

export type HardModeReading =
  { readable: true; hardMode: boolean } | { readable: false; reason: string };

type SavedData = Pick<Storage, 'length' | 'key' | 'getItem'>;
interface Facts {
  hardMode: boolean;
  rows?: number;
}

const ENTRY = /^games-(state|settings)-wordleV2\//;

// The usable facts in one entry: { states: [{ data: { hardMode, currentRowIndex } }] }.
// Anything shaped differently is skipped, not trusted.
function facts(text: string | null): Facts[] {
  try {
    const states: unknown = JSON.parse(text ?? '').states;
    if (!Array.isArray(states)) return [];
    return states.flatMap((state) => {
      const data = state?.data;
      if (typeof data?.hardMode !== 'boolean') return [];
      return [{ hardMode: data.hardMode, rows: data.currentRowIndex }];
    });
  } catch {
    return [];
  }
}

// One answer when every value agrees; none when they disagree or there are none.
function agreed(values: boolean[]): boolean | undefined {
  return values.length > 0 && values.every((value) => value === values[0]) ? values[0] : undefined;
}

export function readHardMode(storage: SavedData, finishedRows: number): HardModeReading {
  const games: Facts[] = [];
  const settings: Facts[] = [];
  for (let i = 0; i < storage.length; ++i) {
    const key = storage.key(i) ?? '';
    const kind = ENTRY.exec(key)?.[1];
    if (kind) (kind === 'state' ? games : settings).push(...facts(storage.getItem(key)));
  }
  if (games.length === 0 && settings.length === 0) {
    return { readable: false, reason: 'no saved Wordle game or settings' };
  }

  const today = agreed(
    games.filter((game) => game.rows === finishedRows).map((game) => game.hardMode),
  );
  if (today !== undefined) return { readable: true, hardMode: today };
  const setting = agreed(settings.map((entry) => entry.hardMode));
  if (setting !== undefined) return { readable: true, hardMode: setting };
  const why = settings.length === 0 ? 'there are no saved settings' : 'the saved settings disagree';
  return {
    readable: false,
    reason: `no saved game matches the ${finishedRows} finished rows, and ${why}`,
  };
}
