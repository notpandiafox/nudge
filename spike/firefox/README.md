# nudge spike (throwaway)

A tiny Firefox extension that answers three questions before the extension's
spec is written:

1. Does the WebAssembly solver load inside a Firefox extension?
2. How does the NYT Wordle page show its tiles, and can they be read?
3. Where is the hard-mode setting stored?

It only reads the page; it never types or clicks anything.

## Run it

1. Build the WebAssembly module (`cmake --build build-wasm`), then copy
   `build-wasm/solver/wasm/nudge.mjs` and `nudge.wasm` into this folder.
2. In Firefox, open `about:debugging#/runtime/this-firefox`, click
   **Load Temporary Add-on…**, and pick this folder's `manifest.json`.
   (It stays loaded until Firefox closes.)
3. If the board can't be read: open `about:addons` → nudge spike →
   Permissions, and allow `nytimes.com`. Firefox asks for this separately for
   Manifest V3 extensions.
4. Open https://www.nytimes.com/games/wordle/index.html, play two or three
   guesses, then click the extension's button (in the puzzle-piece menu).
5. Copy the report from the popup and paste it into the chat. Also try it once
   with hard mode on and once with it off (NYT's settings, gear icon).
