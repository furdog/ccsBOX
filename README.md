# Interface between ccs32clara (foccci) and renault ZE

CAN Interface between CcsBOX and FOCCCI hardware based on ccs32clara.
Based on **esp32c6 super mini** board (at the moment of creation).

> [!IMPORTANT]
> The project is on early stage of development and is a subject to change.

> [!IMPORTANT]
> THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
> APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
> HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY
> OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
> THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
> PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM
> IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF
> ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

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
can r ChargeCurrent  1911 47 -16 0.5 0
can r soc            1911 31 -8  1.0 0
can r enable         1911 23 -8  1.0 0
can r TargetVoltage  1911 15 -16 0.5 0
```

### May i charge RENAULT ZE car without ccsBOX hardware?

- YES! In fact, ccs32clara (foccci) can be configured in real time to listen
  RENAULT ZE frames. For more info see `https://openinverter.org/wiki/Foccci`

> [!CAUTION]
> For standalone operation `TargetVoltage` parameter is omited.
> RENAULT ZE Does not transmit such parameter via CAN.
>
> Stock ccs32clara WILL NOT START charging without this parameter.
> In order to make it work, change `TargetVoltage` definition in ccs32clara
> project and setup target voltage to 395 volts manually after flash:

```diff
-    TESTP_ENTRY(CAT_CHARGE,  TargetVoltage,        "V",       0,      1000,   0,      3   ) \
+    PARAM_ENTRY(CAT_CHARGE,  TargetVoltage,        "V",       0,      1000,   0,      3   ) \
```

After flash write into serial debug window:

```
set TargetVoltage 395
save
```

Foccci (ccs32clara) configuration(CAN mapping). There it is:

```sh
can r ChargeCurrent  1060 23 -8 1.31 0
can r soc            341 43 -12 0.03 0
can r BatteryVoltage 341 63 -16 0.50 0
can r enable         1061 4 -1  1.00 0
```

> [!NOTE]
> In standalone operation (without ccsBOX) you can't charge normally.
> Only if ignition is ON (charging via recuperation).
>
> Car won't get into normal charge mode.
>
> You can't stop nor limit charger output
> (it will be limited automatically by vehicle. See `ChargeCurrent` param)

All the configuration can be written to foccci via serial debug
or openinverter web interface as well.

## TODO

- Add CANopen implementation for better communication with foccci
  (Possibly merge with openinverter web interface?).

## Implementation notes

> 09.11.2026

- First release (not tested on real hardware)

- Tested on bench. Works as intended.

> 11.11.2026

- Adding `TargetVoltage` parameter to ccsBOX
