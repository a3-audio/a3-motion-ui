# A³ Motion UI

The touchscreen app of [A³ Motion](https://github.com/a3-audio/a3-motion), the
motion sampler of [A³ Audio](https://github.com/a3-audio/a3-system). It records
movements on a sphere, plays them back in time with the beat and sends each
channel's position to A³ Core over OSC. JUCE 9 / C++17; it is the `ui`
submodule of a3-motion.

**Documentation: https://a3-audio.github.io/a3-doc/**

- [A³ Motion user guide](https://a3-audio.github.io/a3-doc/user/a3motion.html)
- [Development](https://a3-audio.github.io/a3-doc/development/moc.html) and
  [Configuration](https://a3-audio.github.io/a3-doc/configuration/moc.html)
- [OSC reference](https://a3-audio.github.io/a3-doc/ressources/osc.html).
  Addresses, ports and hosts come from a3-core's `a3-osc.json`, not from
  `config.json`.

**For developers: [ARCHITECTURE.md](ARCHITECTURE.md)** explains the build, the
tests and the code.

## Dependencies

On Debian:

```bash
sudo apt install build-essential cmake pkg-config git \
    xorg-dev libasound2-dev libfreetype-dev libcurl4-openssl-dev libegl-dev \
    libgsl-dev libserial-dev libgpiod-dev \
    googletest libgtest-dev libgmock-dev
```

Older releases call the freetype package `libfreetype6-dev`. Install
`libegl-dev` before the first configure (see ARCHITECTURE.md). `ccache` is used
when it is installed.

The app builds against **JUCE 9.0.1**, installed to `~/local/juce`:

```bash
git clone https://github.com/juce-framework/JUCE.git ~/src/JUCE
cd ~/src/JUCE && git checkout 9.0.1
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/local/juce
cmake --build build --target install
```

## Build, test, run

```bash
./build.sh        # Release build; -d Debug, -c clean, -s restart the service
./test.sh         # build the tests, then run them
./run.sh          # run it (the Debug build if there is one, else Release)
```

The files the machine needs (user service, i3 config, X rules for the
touchscreen) are in [`platform_config/`](platform_config/README.md), with
where each one goes.

## License

REUSE-compliant: the licenses are in `LICENSES/`, which file has which is in
`.reuse/dep5`.
