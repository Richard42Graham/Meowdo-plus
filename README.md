# meowdo – a cat todo list for your terminal

[![CI](https://github.com/Sycorlax/Meowdo/actions/workflows/build.yml/badge.svg)](https://github.com/Sycorlax/Meowdo/actions/workflows/build.yml)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

meowdo is a cute, keyboard-driven todo list with a cat sidekick.  
It runs in your terminal, supports tags, search, pinning, and saves everything in `~/.local/share/meowdo/meowdo.json`.

![SmartStatus screenshot](Meowdo.png)
*(A colorful ncurses UI with a cat and tasks)*

## Features

- **tasks** – add, edit, delete, mark done
- **descriptions** – every task can have a multiline description (`m` to edit, `v` to view)
- **notes** – a separate NOTES tab for free-form multiline notes (`Tab` to switch)
- **scheduled** – hide tasks until their due time (press `S`); they live in a SCHEDULED tab and appear at the top of the list when due
- **pinning** – keep important tasks on top
- **tags** – assign tags, filter by 1–6, color-coded
- **search** – live filtering with `/`
- **progress bar** – shows how many tasks are done
- **freaky cat** – changes mood depending on pending tasks
- **celebration** – all tasks done? kitty party!
- **persistent** – data saved to `~/.local/share/meowdo/meowdo.json`
- **vim-like keys** – `j`/`k`, `g`/`G`, `PgUp`/`PgDn`

## Dependencies

- `ncurses` (development libraries)
- C compiler (`gcc`, `clang`, …)
- `make`

### Install ncurses (examples)

| OS | Command |
|----|---------|
| Arch Linux | `sudo pacman -S ncurses` |
| Debian/Ubuntu | `sudo apt install libncurses-dev` |
| macOS (Homebrew) | `brew install ncurses` |
| Fedora | `sudo dnf install ncurses-devel` |

## Installation

### Arch Linux (AUR)
```bash
yay -S meowdo
```
Or any other AUR helper. Package maintained by [Just_fauna](https://aur.archlinux.org/packages/meowdo).

### From source
```bash
git clone https://github.com/Sycorlax/Meowdo.git
cd Meowdo
make
```

### Nix/NixOS
*(see [Build](#build) section below)*


### Build

```bash
git clone https://github.com/Sycorlax/Meowdo.git
cd Meowdo
make
```

This will compile the program using the provided `Makefile`.  
**Do not run as root** – the program writes to `~/.local/share/meowdo/`.

#### Nix/NixOS

There are two methods to build Meowdo using Nix: the flake and the package.

Note that the package (`default.nix`) will always do the latest tagged release, which as of writing is `1.3.5`. The flake is for the latest development commit (or "unstable") version.

If using the Nix flake, you will first need to [install Nix](https://nixos.org/download/) and enable Nix flakes.

If not on NixOS, add this to `~/.config/nix/nix.conf`:

```nix
experimental-features = nix-command flakes
```

And then run `nix build .` in the project folder.

If on NixOS, add this to your configuration:

```nix
nix.settings.experimental-features = [
    "nix-command"
    "flakes"
];
```

And rebuild, then run `nix build .` in the project folder.

Of using the package (`default.nix`), do this command:

```nix
nix-build -E 'with import <nixpkgs> { }; callPackage ./default.nix {}'
```

This will then build the package.

Both methods create `./result/bin/meowdo`, which can be run.

Note that getting this into the [nixpkgs](https://github.com/NixOS/nixpkgs/) repo is in the works by [Dusk](https://github.com/XDuskAshes/), aka the person writing this nix section.

## Usage

```bash
./meowdo
```

## Default Key Bindings

| Key | Action |
|-----|--------|
| `Tab` | cycle TASKS → SCHEDULED → NOTES (Shift-Tab back) |
| `j` / `↓` | move down |
| `k` / `↑` | move up |
| `J` / `K` | move selected task/note down / up |
| `g` / `G` | jump to top / bottom |
| `PgUp` / `PgDn` | page up / down |
| `n` | new task / new note / new scheduled task |
| `e` | edit selected task / note |
| `m` | edit task description; edit note in NOTES tab |
| `Enter` | open memo — edit if empty, view otherwise |
| `v` | view description / note full-screen |
| `Space` | toggle done |
| `p` | toggle pin |
| `t` | set / clear tag |
| `S` | schedule task (hidden until then) |
| `u` | release scheduled task now |
| `d` | delete selected task / note |
| `D` | delete ALL tasks / notes / scheduled |
| `/` | search |
| `Esc` | clear search + tag filter |
| `1` … `6` | filter by tag (color-coded) |
| `0` | show all tasks |
| `q` | quit |

## Mouse

meowdo supports mouse input in xterm-compatible terminals (and inside tmux
with `set -g mouse on`):

| Action | Result |
|--------|--------|
| click a task / note row | select it |
| double-click a task / note | open its memo (edit if empty, view otherwise) |
| scroll wheel | move the selection up / down |
| click a tab label in the top bar | switch tab |

## Scheduling tasks

Press `S` on a task (or `n` in the SCHEDULED tab) and enter a time. Accepted
formats:

| Input | Meaning |
|-------|---------|
| `+30m`, `+2h`, `+3d`, `+1w` | relative from now (combinable: `+1d 2h`) |
| `18:30` | today at 18:30 (tomorrow if already past) |
| `today 18:30` / `tomorrow 09:00` | explicit day |
| `2026-11-25` / `2026-11-25 09:00` | explicit date |

Scheduled tasks are hidden from the TASKS list, search and tag filters; they
live in the SCHEDULED tab (soonest first) with a countdown. When the time
arrives they appear at the top of the TASKS list — the app checks the clock
every 30 seconds while open, so no cron job or external script is needed.
`u` releases a scheduled task immediately.

## Data Storage

- **Directory:** `~/.local/share/meowdo/`
- **File:** `meowdo.json` (version 1.4.0 and later)

### Format:

```json
{
  "version": 1,
  "tasks": [
    {
      "pinned": false, "done": false,
      "tag": "work", "text": "review PR",
      "desc": "multi\nline description",
      "created": 1678896000, "done_at": 1678899600,
      "sched_at": 0
    }
  ],
  "notes": [
    { "text": "free-form multiline note", "created": 1678896000 }
  ]
}
```

`sched_at` is a unix timestamp — while it is in the future the task is hidden
in the SCHEDULED tab and only becomes visible in the TASKS list once the
time has passed.

### Migrating from older versions

Older meowdo versions stored tasks in a plain-text `todos.txt`
(`<P|->|<x| >|<tag>|<text>|<created_ts>|<done_ts>` per line). If
`meowdo.json` is missing but `todos.txt` is present, meowdo imports it
automatically on first launch and writes `meowdo.json`. The legacy
`todos.txt` is left untouched as a backup. Note that versions before
1.4.0 cannot read `meowdo.json`.

## Notes

- Tags are case-insensitive (stored lowercased)
- Maximum 1024 todos and 512 notes (increase `MAX_TODOS` / `MAX_NOTES` if needed)
- Task descriptions and notes support up to 8 lines of 64 characters each

## Troubleshooting

### 🐱 I don't see the cat, just weird characters!

If your cat looks like `\xe2\xa0\x80\xe2\xa0\x80...` or random symbols, your terminal isn't set to UTF-8 mode. The cat art uses Unicode braille characters that require UTF-8 encoding.

**Quick fix:**

```bash
# Set UTF-8 locale for this session
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

# Run meowdo again
./meowdo
```

**Permanent fix** (add to your `~/.bashrc` or `~/.zshrc`):

```bash
echo 'export LANG=en_US.UTF-8' >> ~/.bashrc
echo 'export LC_ALL=en_US.UTF-8' >> ~/.bashrc
source ~/.bashrc
```

**If locale is missing** (Debian/Ubuntu):

```bash
sudo dpkg-reconfigure locales
# Select en_US.UTF-8 from the list
```

**Test if UTF-8 is working:**

```bash
echo "🐱"
```

If you see a cat emoji, you're good! If you see `\xf0\x9f\x90\xb1`, UTF-8 is not enabled.

### 🖥️ Terminal still not showing the cat?

Some minimal terminal emulators don't support Unicode braille characters. Try using:
- **Linux:** GNOME Terminal, Konsole, Alacritty, Kitty
- **macOS:** Terminal.app, iTerm2
- **Windows:** Windows Terminal, WSL with Windows Terminal

### ❌ "ncurses.h: No such file or directory"

You're missing the ncurses development libraries. See the [Dependencies](#dependencies) section above for installation commands.

### ⚠️ "_XOPEN_SOURCE" redefined warning

This is harmless! The program will still work fine. It's just a compiler warning about a duplicate definition.

## Contributing

Pull requests are welcome! Please ensure:
- Code compiles without warnings (`-Wall -Wextra`)
- The GitHub Actions build passes

## Acknowledgements

- [Bertieio](https://github.com/Bertieio/) – AUR packaging and maintenance
- [Dusk](https://github.com/XDuskAshes/) – Nix flake & package support, working on nixpkgs submission (currently stalled, as contributing to nixpkgs is difficult okay 3: -Dusk)
 
## License

[GPL-3.0](LICENSE) – see LICENSE file for details.

---

Made with 💖 and 😺
