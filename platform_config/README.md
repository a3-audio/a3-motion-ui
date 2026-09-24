# platform_config

What this repository expects the machine to be set up with. Nothing here is
installed by the build — these are files that have to be put in place once, by
hand, and this file says where each one goes.

That is the point of writing it down: the unit calls a script by an absolute
path, and until 2026-09-24 nothing in the repository said that the script it
calls is the one sitting next to it. It was reported as *"the service calls a
start-up script that exists on no machine but this one"*, which was wrong — the
script was here all along, under a different name, with no installation step and
no note connecting the two.

## What goes where

| File | Destination | Notes |
|---|---|---|
| `a3-motion.service` | `~/.config/systemd/user/a3-motion.service` | the unit that runs the app in the booth |
| `a3-wait-for-the-screen` | `~/.local/bin/a3-wait-for-the-screen` | called by the unit's `ExecStartPre`; must be executable |
| `i3/config` | `~/.config/i3/config` | brings the panel up and rotates it |
| `etc/X11/xorg.conf.d/99-ilitek-rotation.conf` | `/etc/X11/xorg.conf.d/` | matches the touch input to the rotated screen |
| `etc/X11/xorg.conf.d/99-ilitek-no-mouse-emulation.conf` | `/etc/X11/xorg.conf.d/` | stops the panel also acting as a mouse |

The two under `etc/` need root; the rest do not.

```sh
install -Dm755 platform_config/a3-wait-for-the-screen ~/.local/bin/a3-wait-for-the-screen
install -Dm644 platform_config/a3-motion.service      ~/.config/systemd/user/a3-motion.service
install -Dm644 platform_config/i3/config              ~/.config/i3/config
systemctl --user daemon-reload
```

## Why the unit waits for the screen

`ExecStartPre` runs `a3-wait-for-the-screen`, which waits for the DPI X reports
to settle below 144 before letting the app start. The reasoning is in the
script's own header and is worth reading before changing the timing: JUCE takes
its scale factor once at start-up from the DPI, and nothing ever revisits it, so
starting while the panel is still coming up gives an interface drawn at half
size and stretched — for the whole session.

There is no user-level `graphical.target` on this machine and
`graphical-session.target` is never reached under lightdm+i3, which is why the
unit waits on a measured quantity rather than on a target.

## Two things this does not do

- **Nothing verifies these are in sync.** A file edited on the machine and not
  here drifts silently; the unit and the script were identical when this was
  written, and nothing keeps them that way.
- **There is no installer.** Adding one is a decision rather than an oversight:
  it would need to decide about root, about backing up what it replaces, and
  about a machine that is not this one.
