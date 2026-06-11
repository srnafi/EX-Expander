# EX-Expander

> A lightweight Windows text expander that replaces typed shortcodes with emoji or custom text in real time — system-wide, across any application.

---

## What It Does

EX-Expander runs silently in the background and watches your keyboard input. When you type a configured shortcode (e.g. `:smile`), it automatically replaces it with the corresponding expansion (e.g. 😊) — without any extra keystrokes or context-switching.

Think of it as autocorrect you fully control, built natively for Windows.

---

## Features

- **Real-time keyboard hook** — intercepts typed tokens before they reach any application
- **Configurable trigger character** — use `:`, `;`, or any symbol you prefer
- **Space or symbol triggering** — expansions fire on spacebar, punctuation, or custom symbols
- **Popup suggestion menu** — shows matching expansions as you type, à la emoji pickers
- **Scope/app filtering** — restrict expansions to specific applications or use them globally
- **Persistent storage** — all expansions and settings stored in a local SQLite database
- **Auto-start on login** — optional Windows registry integration for startup
- **Settings UI** — configure everything through an embedded HTML/JS settings panel
- **Clipboard-safe replacements** — saves and restores clipboard state around paste operations

---

## Tech Stack

| Layer | Technology |
|---|---|
| Core engine | C++ (Win32 API) |
| UI shell | HTML, CSS, JavaScript (embedded WebView) |
| Database | SQLite (via bundled `external/sqlite`) |
| Build system | Visual Studio (`.vcxproj` / `.slnx`) |
| Installer | NSIS or custom (see `installer/`) |

---

## Project Structure

```
EX-Expander/
├── src/
│   ├── core/          # Keyboard hook, token matching, replacement logic
│   ├── ui/            # Popup window, animation, WebView host
│   └── data/          # SQLite database access layer
├── include/           # Shared headers (Globals.h, Settings, etc.)
├── external/sqlite/   # Bundled SQLite amalgamation
├── assets/            # Icons, images, UI assets
├── installer/         # Installer scripts/configuration
├── ANALYSIS.md        # Detailed code analysis and refactoring guide
├── EX-Expander.vcxproj
└── EX-Expander.slnx
```

---

## Getting Started

### Prerequisites

- Windows 10 or later
- Visual Studio 2022 (with C++ Desktop workload)
- No external runtime dependencies — SQLite is bundled

### Build

1. Clone the repository:
   ```bash
   git clone https://github.com/srnafi/EX-Expander.git
   cd EX-Expander
   ```

2. Open `EX-Expander.slnx` in Visual Studio 2022.

3. Select the desired configuration (`Debug` or `Release`) and build with **Ctrl+Shift+B**.

4. The compiled binary will appear in the output directory (typically `x64/Release/`).

### Run

Launch `EX-Expander.exe` directly. It will appear in the system tray. Use the tray icon to open settings or exit.

---

## How It Works

1. **Keyboard Hook** — `KeyboardHook.cpp` installs a low-level `WH_KEYBOARD_LL` hook to capture all keystrokes.
2. **Token Buffer** — Typed characters are accumulated into an input buffer. On a trigger event (space or symbol), the buffer is checked against the expansion database.
3. **Matching** — `EmojiMatcher` queries SQLite for prefix matches, then confirms an exact match before firing.
4. **Replacement** — `EmojiReplacer` simulates backspace keystrokes to erase the token, saves the current clipboard, pastes the expansion via `Ctrl+V`, and restores the clipboard asynchronously.
5. **Popup UI** — If multiple matches exist, a popup window is shown near the cursor with a ranked list of suggestions.

---

## Configuration

All settings are accessible through the built-in settings panel (tray icon → Settings):

| Setting | Description | Default |
|---|---|---|
| Trigger character | Symbol that starts an expansion (e.g. `:`) | `:` |
| Insert trigger | Fire on `space` or on symbol | `space` |
| Max popup items | Max suggestions shown in popup | `5` |
| Popup position | `fixed` or cursor-relative | `fixed` |
| Scope mode | `global` or `block` (per-app filtering) | `global` |
| Auto-start | Launch EX-Expander on Windows login | `false` |

Settings are persisted to the local SQLite database.

---

## Architecture Notes

See [`ANALYSIS.md`](ANALYSIS.md) for a detailed breakdown of the codebase, including:

- Identified code duplication and recommended refactors
- Error handling gaps in the database layer
- Clipboard race condition analysis
- A phased refactoring plan (from quick wins to full OOP migration)
- Code metric targets (test coverage, duplicate code, global state reduction)

---

## Known Limitations & Roadmap

- [ ] No unit test coverage yet (see ANALYSIS.md Phase 3 for dependency injection plan)
- [ ] Global state scattered across `Globals.h` — planned migration to `Application` singleton
- [ ] Clipboard restore uses a hardcoded 3000ms delay
- [ ] Registry access is not yet wrapped in a reusable helper class

**Planned improvements (priority order):**
1. Extract shared utilities (`ClipboardGuard`, `FindExactMatch`, `Settings` struct)
2. Improve error handling in the database layer
3. Introduce dependency injection for testability
4. Refactor UI management into `PopupManager` class

---

## Contributing

Contributions are welcome. If you'd like to help:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -m 'Add your feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Open a Pull Request

Please read [`ANALYSIS.md`](ANALYSIS.md) before contributing — it documents the current architecture, known issues, and where effort is most needed.

---

## License

This project does not currently include a license file. All rights reserved by the author unless otherwise stated.

---

## Author

**srnafi** — [github.com/srnafi](https://github.com/srnafi)
