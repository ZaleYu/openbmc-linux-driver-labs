# Linux source map

| Path | Study focus |
|---|---|
| `drivers/char/ipmi/kcs_bmc_aspeed.c` | register mapping, host I/O decode, SIRQ, BMC IRQ |
| `drivers/char/ipmi/kcs_bmc.c` | device/client registration and ownership |
| `drivers/char/ipmi/kcs_bmc_cdev_ipmi.c` | KCS IPMI state machine and char-device UAPI |
| `drivers/char/ipmi/kcs_bmc_serio.c` | alternate serio client |
| `drivers/char/ipmi/kcs_bmc_npcm7xx.c` | Nuvoton controller comparison |
| `drivers/char/ipmi/ipmi_kcs_sm.c` | host-side KCS state machine |
| `include/uapi/linux/ipmi_bmc.h` | SMS_ATN and force-abort ioctls |
| `Documentation/devicetree/bindings/ipmi/aspeed,ast2400-kcs-bmc.yaml` | binding contract |

## Trace one host command

1. Host writes WRITE_START, request bytes, WRITE_END and final byte.
2. ASPEED interrupt reaches `kcs_bmc_aspeed` and the registered client.
3. `kcs_bmc_cdev_ipmi` advances WRITE phases and wakes its wait queue.
4. kcsbridge polls and reads the complete request from `/dev/ipmi-kcsN`.
5. host-ipmid dispatches NetFn/command to a provider or D-Bus service.
6. kcsbridge writes NetFn/command/completion/data as one response.
7. Kernel enters READ phase and supplies bytes when the host sends READ_BYTE.

## Typical change locations

- New board/address/channel: DTS and service instance.
- ASPEED register or reset defect: `kcs_bmc_aspeed.c`.
- Generic phase/buffer problem: `kcs_bmc_cdev_ipmi.c` and its tests.
- Slow command/provider: kcsbridge, host-ipmid, or the target D-Bus service.
- eSPI OOB/Flash/VW support: exact platform kernel driver, binding and UAPI.

