# Interface between ccs32clara (foccci) and renault ZE

CAN Interface between CcsBOX and FOCCCI hardware based on ccs32clara.
Based on **esp32c6 super mini** board (at the moment of creation).

> [!IMPORTANT]
> The project is on early stage of development and is a subject to change.

## What does it do?

- Monitors RENAULT ZE battery, (see `src/kangoo_bms_mon.h`).
- Forwards it to foccci via CAN interface. (see `src/foccci_can_interface.h`).
- Receives data from foccci via CAN interface and forwards it to upper software layers (see `ccsbox.h`).
- Fakes RENAULT ZE charging mode. So the vehicle thinks it's being normally charged.
- Physically disconnects BCB module from other systems, by filtering CAN messages.

## How?

- esp32c6 or other dual CAN/TWAI compatible board. The board will work as MITM bridge.
- ccsBOX is physically placed between BCB(onboard charger) and other systems (bridged).
- Preconfigured CAN mapping on ccs32clara (foccci)

### What's the foccci configuration (CAN mapping)?

- Message ID: 0x778 (Foccci transmits to ccsBOX)

```sh
can t PortState 1912 63 -8 1.0 0
```

- Message ID: 0x777 (Foccci receives from ccsBOX)

```sh
can r BatteryVoltage 1911 63 -16 0.5 0
can r ChargeCurrent 1911 47 -16 0.5 0
can r soc 1911 31 -16 1.0 0
can r enable 1911 15 -16 1.0 0
```

For standalone operation without ccsBOX, use next config for foccci:

```sh
can r ChargeCurrent 1060 23 -8 1.31 0
can r soc 341 43 -12 0.03 0
can r BatteryVoltage 341 63 -16 0.50 0
can r enable 1061 4 -1 1.00 0
```

All the configuration can be written to foccci via serial debug
or openinverter web interface as well.

## TODO

- Add CANopen implementation for better communication with foccci
  (Possibly merge with openinverter web interface?).

## Implementation notes

> 09.11.2026

- First release (not tested on real hardware)
