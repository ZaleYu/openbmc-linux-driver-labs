# Userspace GPIO Tools

Modern userspace should use the GPIO character-device interface and libgpiod.
The deprecated sysfs GPIO ABI (`/sys/class/gpio/export`) and global integer
numbering should not be the basis of new code.

## Inventory

With libgpiod 2.x:

```sh
gpiodetect
gpioinfo
gpioinfo -c gpiochip0
gpioinfo -c gpiochip0 17
```

Inspect:

- Chip device and label.
- Local line offset.
- Device Tree line name.
- Direction, active-low, bias, drive, edge, and debounce.
- Consumer name and whether the line is already requested.

CLI syntax differs between libgpiod 1.x and 2.x. Run `<tool> --version` and
`<tool> --help` on the target rather than blindly copying a command.

## Read an input

```sh
gpioget --numeric -c gpiochip0 17
gpioget --numeric -c gpiochip0 PRESENCE_N
```

`gpioget` requests the line while reading it. It can fail with `EBUSY` if a
kernel driver or daemon already owns the line. That failure is useful ownership
evidence, not a reason to force access.

## Monitor edges

```sh
gpiomon -c gpiochip0 -e both 17
gpiomon -c gpiochip0 -e falling PRESENCE_N
```

Events include a timestamp. The default kernel ABI v2 clock is monotonic unless
another event clock is selected. Sequence numbers in the direct ABI can reveal
lost/overwritten events or ordering across multiple requested lines.

## Drive an output

```sh
gpioset -c gpiochip0 RESET_N=active
```

For an active-low request, `active` may correspond to a low electrical level.
`gpioset` holds the line while the process runs; the state after process exit is
not guaranteed. Use the tool's actual version-specific mode/hold options.

Do not drive a line until its electrical direction, ownership, and safe states
are verified. A one-command GPIO write can power off a host, reset a CPLD,
disable write protection, or short two outputs.

## Line information changes

Recent libgpiod provides `gpionotify` to observe requests, releases, and
reconfiguration:

```sh
gpionotify -c gpiochip0 17
```

This is useful when a line unexpectedly becomes busy or changes direction.

## Direct ABI v2 programs

`monitor_gpio_v2.c` demonstrates:

- `GPIO_V2_GET_LINE_IOCTL`.
- Initial logical value read.
- Rising/falling edge events.
- Timestamp, global sequence, and per-line sequence.
- `poll()` and clean termination.

`set_gpio_v2.c` demonstrates an atomic initial output value in the line request,
optional active-low semantics, and a bounded hold time.

The direct ioctl ABI is useful for understanding the kernel contract. Normal
applications should generally prefer libgpiod for portability and convenience.

## Kernel versus userspace ownership

Choose one functional owner:

- Kernel driver for a standard subsystem, tight kernel integration, or hardware
  sequencing that must exist before userspace.
- OpenBMC/libgpiod daemon for platform policy, inventory presence, and service
  actions that do not require a dedicated kernel ABI.

Two owners cannot independently request the same exclusive line. Do not work
around this with raw register writes.

