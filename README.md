# EX-Expander

> A lightweight Windows text expander that replaces typed shortcodes with emoji or custom text in real time — system-wide, across any application.

<!-- Replace the line below with your actual GIF once recorded -->
![EX-Expander demo](assets/demo.gif)

---

## Download

Head to the [**Releases**](https://github.com/srnafi/EX-Expander/releases) page and grab the latest `EX-Expander.exe` — no install required, just run it.

> **WebView2 Runtime required.** Most Windows 10/11 machines already have it. If the app doesn't open, download it from [Microsoft's site](https://developer.microsoft.com/en-us/microsoft-edge/webview2/).

---

## What It Does

EX-Expander runs silently in the background and watches your keyboard input. When you type a configured shortcode (e.g. `:smile`), a popup instantly appears — navigate the suggestion list with arrow keys, pick an entry, and the expansion is inserted right where your cursor is, in any application.

Think of it as autocorrect you fully control, built natively for Windows.

---

## Features

- **Real-time keyboard hook** — intercepts typed tokens before they reach any application
- **Instant popup navigation** — arrow-key through suggestions and insert expansions without leaving your flow
- **Inserts anywhere** — works in any focused window: browsers, editors, chat apps, terminals
- **Configurable trigger character** — use `:`, `;`, or any symbol you prefer
- **Space or symbol triggering** — expansions fire on spacebar, punctuation, or a custom symbol
- **Scope/app filtering** — restrict expansions to specific applications or run them globally
- **Persistent storage** — all expansions and settings stored in a local SQLite database
- **Auto-start on login** — optional Windows registry integration for startup
- **Expansion manager UI** — add, edit, and organize your expansions through a built-in WebView2-powered interface
- **Clipboard-safe replacements** — saves and restores clipboard state around every paste operation

---

## Tech Stack

| Layer | Technology |
|---|---|
| Core engine | C++ (Win32 API) |
| Expansion manager UI | WebView2 (HTML, CSS, JavaScript) |
| Database | SQLite (bundled via `external/sqlite`) |
| Build system | Visual Studio (`.vcxproj` / `.slnx`) |
| Installer | See `installer/` |

---

## Project Structure

```
EX-Expander/
├── src/
│   ├── core/          # Keyboard hook, token matching, replacement logic
│   ├── ui/            # Popup window, animation, WebView2 host
│   └── data/          # SQLite database access layer
├── include/           # Shared headers (Globals.h, Settings, etc.)
├── external/sqlite/   # Bundled SQLite amalgamation
├── assets/            # Icons, images, UI assets
├── installer/         # Installer scripts/configuration
├── EX-Expander.vcxproj
└── EX-Expander.slnx
```

---

## Building from Source

### Prerequisites

- Windows 10 or later
- Visual Studio 2022 (with **Desktop development with C++** workload)
- WebView2 SDK (included via `packages.config` — restored automatically by NuGet)
- No other external runtime dependencies — SQLite is bundled

### Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/srnafi/EX-Expander.git
   cd EX-Expander
   ```

2. Open `EX-Expander.slnx` in Visual Studio 2022. NuGet will restore the WebView2 package automatically.

3. Select `Release` | `x64` and build with **Ctrl+Shift+B**.

4. The output binary will be at `x64/Release/EX-Expander.exe`.

---

## How It Works

1. **Keyboard Hook** — installs a low-level `WH_KEYBOARD_LL` hook to capture all keystrokes system-wide.
2. **Token Buffer** — typed characters accumulate in an input buffer; on a trigger event (space or symbol), the buffer is checked against the expansion database.
3. **Matching** — queries SQLite for prefix matches, shows the popup if candidates exist, and waits for user selection.
4. **Popup Navigation** — the user navigates the suggestion list with arrow keys and confirms a selection; the popup closes instantly.
5. **Replacement** — simulates backspace keystrokes to erase the typed token, saves the current clipboard, pastes the expansion via `Ctrl+V`, and restores the clipboard asynchronously.

---

## Configuration

All settings are accessible through the built-in WebView2 settings panel (system tray icon → Settings):

| Setting | Description | Default |
|---|---|---|
| Trigger character | Symbol that begins an expansion (e.g. `:`) | `:` |
| Insert trigger | Fire on `space` or on symbol | `space` |
| Max popup items | Maximum suggestions shown in the popup | `5` |
| Popup position | `fixed` or cursor-relative | `fixed` |
| Scope mode | `global` or `block` (per-app filtering) | `global` |
| Auto-start | Launch EX-Expander on Windows login | `false` |

Settings are persisted to the local SQLite database.

---

## Roadmap

- [ ] Unit test coverage (dependency injection refactor planned)
- [ ] Migrate global state to a single `Application` singleton
- [ ] Configurable clipboard restore delay (currently hardcoded at 3 s)
- [ ] Wrap Registry access in a reusable `RegistryHelper` class
- [ ] Themed popup (light/dark mode)

---

## Contributing

Contributions are welcome!

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes: `git commit -m 'Add your feature'`
4. Push and open a Pull Request

---

## License

This project does not currently include a license file. All rights reserved by the author unless otherwise stated.

---

## Author

**srnafi** — [github.com/srnafi](https://github.com/srnafi)
