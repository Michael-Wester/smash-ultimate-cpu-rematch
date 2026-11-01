# Smash Ultimate CPU Rematch

Restart a CPU fight without returning to character or stage selection. Keep the
same characters, stage and rules, and get straight into another match.

**For Super Smash Bros. Ultimate 13.0.3 on Eden, with Skyline installed.**
Works in regular local Smash with **one player and one CPU**. Version 0.5.0 has
been confirmed working on one Windows/Eden setup, both mid-match and during GAME!.

## Download and install

**[Download v0.5.0](dist/cpu-rematch-13.0.3-v0.5.0.zip?raw=true)**

1. Stop Smash completely.
2. In Eden, select **File → Open Eden Folder**.
3. Extract the ZIP's `sdmc` folder into that folder, merging folders when asked.
   If updating, replace `libcpu_rematch.nro` and keep only one copy of the plugin.
4. Start a regular local match against one CPU.

The installed file should be here, inside your Eden folder:

```text
sdmc/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libcpu_rematch.nro
```

Skyline is required and is a separate download from
[Skyline releases](https://github.com/skyline-dev/skyline/releases). Its `exefs`
folder, containing `main.npdm` and `subsdk9`, must sit beside the `romfs` folder
above. ARCropolis is not required.

## How to rematch

Hold the two shoulder buttons and tap the face button:

| Controller | Shortcut |
| --- | --- |
| Switch Pro | **L + R + Y** |
| Xbox with the tested Eden mapping | **LB + RB + X** |

These are the upper shoulder buttons, not the lower triggers. With a custom
mapping, use whichever buttons Eden assigns to **Switch L, R and Y**.

Use the shortcut after GO or immediately during **GAME!**, before the results
screen loads. Release it before trying again. The normal reload/countdown still
plays. Training, online play and the results screen are not supported.

## If it does nothing

- Check that Smash is **13.0.3**, Skyline is installed and the plugin is at the path above.
- Check your controller mapping in Eden.
- After stopping the game, check `sdmc/cpu_rematch.log`. When reporting a bug,
  include that log, your Eden version and whether you pressed mid-match or during GAME!.

No network access is needed for rematching. To uninstall, stop the game and delete
`libcpu_rematch.nro`. Restarting can skip normal result recording for that match.

## Source and license

See [building and testing](docs/DEVELOPMENT.md), [validation notes](docs/VALIDATION.md)
and [credits](CREDITS.md). Source is licensed under [AGPL-3.0-or-later](LICENSE).
This is an independent community mod; no game files, firmware or keys are included.
