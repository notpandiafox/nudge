// The page script on NYT's Wordle page (plan §2): watches the board and draws
// the panel; the background script answers its hint requests.
import type { HintResponse } from '../messages';
import { watchPage } from './page';
import { createPanel } from './panel';

watchPage({
  document,
  storage: localStorage, // NYT's saved data: the content script shares the page's origin
  panel: createPanel(document),
  strategy: 'minimax', // the strategy switch comes in E8
  ask: (request) => browser.runtime.sendMessage(request) as Promise<HintResponse>,
});
