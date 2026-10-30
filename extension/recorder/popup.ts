// The recorder's popup: asks the Wordle tab for a copy, shows it, and offers
// it as a file to put into extension/test/fixtures/.
import type { Fixture } from './fixture';

const note = document.querySelector<HTMLInputElement>('#note')!;
const save = document.querySelector<HTMLAnchorElement>('#save')!;
const status = document.querySelector<HTMLParagraphElement>('#status')!;
const copy = document.querySelector<HTMLTextAreaElement>('#copy')!;

function fileName(fixture: Fixture): string {
  const slug = fixture.note
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-|-$/g, '');
  return `wordle-${fixture.recordedOn}-${slug || 'board'}.json`;
}

document.querySelector('#record')!.addEventListener('click', async () => {
  status.className = '';
  try {
    const [tab] = await browser.tabs.query({ active: true, currentWindow: true });
    const fixture: Fixture = await browser.tabs.sendMessage(tab!.id!, {
      type: 'record',
      note: note.value.trim(),
    });
    if (fixture.boardHtml === null) throw new Error('no board rows on this page');
    const json = `${JSON.stringify(fixture, null, 2)}\n`;
    copy.value = json;
    save.href = URL.createObjectURL(new Blob([json], { type: 'application/json' }));
    save.download = fileName(fixture);
    save.hidden = false;
    status.textContent = `Recorded ${Object.keys(fixture.storage).length} saved entries. Check it, then save it.`;
  } catch (error) {
    status.className = 'problem';
    status.textContent = `Couldn't record (open the Wordle page first): ${error}`;
  }
});
