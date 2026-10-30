// The background script: hosts the solver (plan E-D2, E-D3) and runs the
// toolbar button (plan E8). The page script asks it for hints; it answers with
// solverHost.
import type { HintRequest, HintResponse, Toggle, WordlePage } from '../messages';
import { answer, type SolverModule } from './solverHost';

// Loaded once, on the first request, from the extension's own copy of the
// solver (built into dist/ by build.mjs). By its URL, not an import, so the
// bundle doesn't try to include it. A failed load is forgotten, so the next
// request tries again instead of failing for good.
let solver: Promise<SolverModule> | undefined;
function loadSolver(): Promise<SolverModule> {
  solver ??= import(browser.runtime.getURL('nudge.mjs'))
    .then((module: { default: () => Promise<SolverModule> }) => module.default())
    .catch((error: unknown) => {
      solver = undefined;
      throw error;
    });
  return solver;
}

type Message = Partial<HintRequest> | WordlePage | undefined;

browser.runtime.onMessage.addListener((message: Message, sender: browser.runtime.MessageSender) => {
  // The manifest's popup ("Nudge") is for every other tab. Firefox puts it back
  // when this tab moves to another page.
  if (message?.type === 'wordle-page') {
    const tabId = sender.tab?.id;
    return tabId === undefined ? undefined : browser.action.setPopup({ tabId, popup: '' });
  }
  if (message?.type !== 'hint') return undefined;
  // Every outcome is one of the four answers, a failed load included.
  return loadSolver().then(
    (module): HintResponse => answer(module, message as HintRequest),
    (error: unknown): HintResponse => ({ kind: 'error', message: `loading the solver: ${error}` }),
  );
});

// Fires only where there's no popup: the Wordle tabs.
browser.action.onClicked.addListener((tab) => {
  if (tab.id !== undefined)
    void browser.tabs.sendMessage(tab.id, { type: 'toggle' } satisfies Toggle);
});
