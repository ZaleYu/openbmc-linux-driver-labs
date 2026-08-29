# Userspace tools

## BMC side

Inventory and ownership checks:

    ls -l /dev/ipmi-kcs*
    ls -l /sys/class/misc/ipmi-kcs*/device
    fuser -v /dev/ipmi-kcs3
    systemctl status phosphor-ipmi-kcs@ipmi-kcs3.service
    systemctl status phosphor-ipmi-host.service

The character device returns one complete host request from `read()`. A response
is written as one complete IPMI response. `poll()` signals a pending request.
The UAPI also provides ioctls to set/clear SMS_ATN and force an abort.

Opening the device activates one KCS client; a second owner should fail. Do not
run `dd`, `hexdump`, or the included lab responder while kcsbridge is active.
Reading a request without writing a valid response leaves the host waiting.

## Host side

Typical checks from the host OS:

    dmesg | grep -Ei 'ipmi|kcs'
    ls -l /dev/ipmi*
    ipmitool mc info
    ipmitool raw 0x06 0x01

The first command proves host enumeration; the IPMI commands test the complete
request/response path. Compare in-band KCS results with network IPMI carefully:
they traverse different front ends even if both reach shared IPMI handlers.

## Included programs

`kcs_inventory` is read-only. `kcs_lab_responder` handles one request and
returns completion code `0xC1` (invalid command). It requires `--lab`, refuses
non-`ipmi-kcs` paths, and is intended only after stopping the production bridge.

