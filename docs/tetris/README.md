<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Tetris

A single-player Tetris game for the FoloToy AI Passport. The board is 10 columns by 20 rows, pieces use a 7-bag randomizer, and the interface is in Chinese. The best score, a single save slot, the sound switch, and the volume are stored in NVS.

## Starting a game

The device boots into the start screen. Use Up/Down to select and OK to confirm:

| Option | Behavior |
| --- | --- |
| Start New Game | Begins a game immediately and overwrites the previous save |
| Continue | Restores the last save and shows its score and lines below the label; shows "No save" and cannot be selected when none exists |
| Settings | Adjusts sound and volume; long-press OK to go back |

Long-press Up during play to pause. The pause menu offers Continue, Settings, and Exit Game. Exit Game saves the current progress and returns to the start screen. The game-over screen offers Play Again or Back to Menu.

## Controls

The device has three buttons; Up/Down move the piece horizontally:

| Screen | Input | Action |
| --- | --- | --- |
| Start, pause, game-over menus | Up/Down short press | Move the selection |
| | OK short press | Confirm |
| Playing | Up short press | Move one cell left |
| | Down short press | Move one cell right |
| | OK short press | Rotate clockwise (with wall kicks) |
| | Hold OK | Fast drop; release to resume normal speed |
| | Long-press Up | Pause and save |
| Paused | Up/Down short press | Move the selection |
| | Long-press Up | Resume immediately |

A short OK press rotates on release. Holding OK for about 0.5 seconds starts the fast drop instead, so that press does not rotate. Releasing OK during a fast drop immediately restores the normal drop speed.

## Rules

- Pieces spawn at the top of the board and lock when they land or touch existing blocks. After locking, full rows are cleared and everything above them shifts down.
- The game ends when a new piece cannot spawn.
- The randomizer is a 7-bag: every seven pieces contain each of the seven shapes exactly once.
- Clearing four lines at once shows a celebration card and plays a dedicated sound. If the same clear also raises the difficulty, the difficulty notice takes priority.

### Scoring

| Lines cleared at once | Score |
| --- | --- |
| 1 | 100 × current difficulty |
| 2 | 300 × current difficulty |
| 3 | 500 × current difficulty |
| 4 or more | 800 × current difficulty (capped at four lines) |

The multiplier uses the difficulty **before** the clear. When a game ends with a score above the best, the best score is saved and "New Record!" is shown.

## Difficulty progression

Difficulty starts at 1 and increases by one **for every 30 cumulative cleared lines**:

```
difficulty = total cleared lines / 30 + 1
```

Each tier-up has the following effects:

- **Faster drop.** The drop interval decreases by 80 ms per tier, down to 140 ms:

  | Difficulty | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8+ |
  | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | Drop interval | 700 ms | 620 ms | 540 ms | 460 ms | 380 ms | 300 ms | 220 ms | 140 ms |

- **No landing preview.** Difficulty 1 draws an outline at the landing position; from difficulty 2 onward the outline is gone.
- **Garbage row.** Starting at difficulty 2, each tier-up appends one full row of blocks at the bottom with a single random gap and shifts the existing board up by one row. Because the row is only added after a line clear, the top row is always empty, so the new row cannot cause an immediate top-out.
- **On-screen notice.** The right information panel shows a card for about 2.5 seconds:

  ```
  Difficulty up!
  Faster drop
  Landing preview off
  Garbage row added
  ```

  A level-up sound plays at the same time. The game keeps running while the card is visible.

Difficulty is not stored directly. It is re-derived from the cleared-line count when a save is loaded, so an old save cannot carry a difficulty that disagrees with its lines.

## Saves and records

- The game autosaves every 8 seconds while playing and also saves immediately on pause and when exiting to the start screen.
- There is one save slot, holding the board, current and next pieces, randomizer state, score, lines, and difficulty. Starting a new game overwrites it.
- A game over deletes the save so that Continue cannot restore a finished game.
- The save carries a CRC and per-field validation; corrupt or incompatible data is rejected instead of being loaded.
- The best score, sound switch, and volume are stored separately in NVS and survive a reboot.

## Settings

| Input | Behavior |
| --- | --- |
| OK short press | Toggle sound on or off |
| Up/Down short press | Change volume by 5%, from 0 to 100; only available while sound is on |
| Long-press OK | Return to the previous screen (start or pause) |

Sound defaults to on and volume to 55%. Turning sound off dims the volume bar but keeps its value; turning sound back on restores it.

## Screen layout

- The header shows the title and battery percentage.
- The board is on the left. The right panel shows the next piece, score, lines, difficulty, best score, and the pause hint.
- The bottom line shows the control hint, or an error message when the buttons are unavailable.
- The difficulty notice and the four-line celebration card both appear in the right panel, never covering the board.
