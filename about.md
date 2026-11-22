# Fast Frame Windows

Shows how many frames you had for every click, fast.

Red is how many frames earlier a click could have been. Blue is how many frames later. Together they make the window.

## Why it is fast

Instead of replaying the whole level again and again, the mod saves the game state a few frames before each click while you play. To measure a click it jumps to that saved state, moves the click, and runs only a short stretch of the level. It finds each side of the window by halving the range rather than trying every shift.

## Use

1. Play a level (normal or practice mode).
2. Pause and press the red **FW** button.
3. Press **Analyze**. Watch the progress bar, or cancel any time.
4. Read the results: a red and blue bar for every click.

## Good to know

- Measured with 240 physics steps per second. Leave TPS at 240 and turn off other bots or physics mods while analyzing.
- Clicks that cannot be reproduced from the saved state are marked and left out of the averages.
- A plus sign means the window was limited by the range tested or by the next click, so it is at least that large.
- Windows are measured by running the level, so they depend on the level behaving the same way each time.

## Credits

The idea comes from Razy's Frame Windows by tristi. This mod is an independent, from-scratch implementation and shares no code or assets with it.
