// Spike popup: (1) does the WebAssembly solver load in an extension page, and
// how fast; (2) what does the board reader see on the Wordle page; (3) what
// hint does the solver give for that board.
import createModule from './nudge.mjs';

const report = { solver: {}, board: null, errors: [] };
const show = (id, text, ok) => {
  const element = document.getElementById(id);
  element.textContent = text;
  element.className = ok ? 'ok' : 'bad';
};

let nudge = null;
try {
  const start = performance.now();
  nudge = await createModule();
  const solver = new nudge.Solver(false);
  report.solver = {
    loadMs: Math.round(performance.now() - start),
    firstGuess: solver.getBestGuess(),
  };
  solver.delete();
  show('solver', `Solver loaded in ${report.solver.loadMs} ms; first guess ${report.solver.firstGuess}.`, true);
} catch (error) {
  report.errors.push(`solver: ${error}`);
  show('solver', `The solver didn't load: ${error}`, false);
}

try {
  const [tab] = await browser.tabs.query({ active: true, currentWindow: true });
  report.board = await browser.tabs.sendMessage(tab.id, { type: 'read-board' });
  const rows = report.board.rows;
  let text = `Read ${rows.length} finished row(s) from ${report.board.tileCount} tiles.`;
  if (nudge && rows.length > 0) {
    const solver = new nudge.Solver(false);
    const statuses = rows.map(({ word, pattern }) => solver.addFeedbackPattern(word, pattern));
    const start = performance.now();
    const hint = solver.getBestGuess();
    report.hint = { statuses, hint, ms: Math.round(performance.now() - start),
                    remaining: solver.getRemainingCount(), fallback: solver.isUsingFallback() };
    solver.delete();
    text += ` Statuses: ${statuses.join(", ")}. Hint: ${hint} (${report.hint.ms} ms, ${report.hint.remaining} left).`;
  }
  show('board', text, true);
} catch (error) {
  report.errors.push(`board: ${error}`);
  show('board', `Couldn't read a board here (open the Wordle page first): ${error}`, false);
}

document.getElementById('report').value = JSON.stringify(report, null, 2);
