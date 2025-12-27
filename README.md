# RealBoy Emulator

RealBoy is a [Game Boy] emulator.

It is intended to be used alongside [RealBoy Monitor] and [libemu] as a teaching device for
introducing low-level and systems-level programming.

Take a look [here] for more details on the purpose and goals of the project.

## Installation

### Compiling from Source

Dependencies:

* meson (for building)
* wayland
* wayland-protocols
* libevdev
* pixman
* [libemu] (optional)

Run these commands:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install

### Adding User to the 'input' Group

In order to show how to interface directly with input event, we directly access the special files
in `/dev/input`.

Most probably you'll get a bunch of `open(): Permission denied` messages when
running RealBoy.

The simplest way to get permissions is to add your user to the `input` group:

	sudo usermod -aG input username

[Game Boy]: https://en.wikipedia.org/wiki/Game_Boy
[RealBoy Monitor]: https://github.com/sergio-gdr/realboy-mon
[libemu]: https://github.com/sergio-gdr/libemu
[here]: https://raw.githubusercontent.com/sergio-gdr/realboy-book/refs/heads/main/book.txt
