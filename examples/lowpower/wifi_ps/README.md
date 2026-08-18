# wifi_ps

Demonstrates **WiFi power save** - an associated station does not have to keep its receiver on.

## How it works

The station tells the access point it is going to doze. The access point then buffers any traffic
for it and delivers it on the next **DTIM beacon**. The station wakes for that beacon, collects
whatever was held, and goes back down.

**A larger DTIM interval means fewer wakeups and lower current, at the cost of latency** - anything
waiting sits through another interval. A beacon is typically 102.4 ms.

This is the low power path for a device that stays connected. A device that never joins a network
takes a different one - see [cpu_sleep](../cpu_sleep/) and
[cpu_deep_sleep](../cpu_deep_sleep/) next door.

## The interface

The whole feature is one call:

```c
tkl_wifi_set_lp_mode(TRUE, dtim);
```

The only other thing the example does is join an access point, because **power save is a property
of an association and does nothing without one**. Platform implementations check that the station
is in STA mode and fail outright otherwise.

## Configuration

| Option | Default | Meaning |
|---|---|---|
| `EXAMPLE_WIFI_SSID` | `your-ssid` | access point to join |
| `EXAMPLE_WIFI_PASSWORD` | `your-password` | its password |
| `EXAMPLE_WIFI_CONNECT_TIMEOUT_S` | 30 | how long to wait for an address |
| `EXAMPLE_WIFI_PS` | y | whether to enable power save; turn off for the comparison |
| `EXAMPLE_WIFI_PS_DTIM` | 1 | DTIM intervals to sleep through, 1-10 |
| cpu sleep | light | `none` / `light` / `deep`, see below |

### The cpu has to sleep too

**Radio power save and cpu sleep are separate savings that add up.** With only the radio dozing
the core still spins in its idle loop at full speed, and on most parts that is the larger share of
what is left - so the radio saving alone reads as no saving at all.

Measured on GD32VW553 (**whole board**, DTIM=1):

| cpu setting | average current |
|---|---|
| `none` (radio power save only) | 45 mA |
| `light` | 44-45 mA |
| **`deep`** | **about 3.6 mA** |

**The deep row moves a great deal with the network.** A completely idle one gets to 2.4 mA; with
other devices on the router sending and receiving it climbs into the low tens. 3.6 mA is what an
ordinary home network gave, which is worth more as a reference than the best case from a quiet lab.

That spread is all radio receive and has nothing to do with the firmware.

Two things worth noting. **An associated device cannot enter light sleep at all on GD32VW553** -
measured, the core never stops, and the 39 mA underneath those 44-45 is simply the current of a
core running flat out rather than any kind of floor.

The platform ties cpu sleep depth to a system-wide power mode, and the one light sleep maps to
means "no power saving" in as many words - so the wifi stack has no reason to give up its claim on
the power manager, and the sleep request is refused every time. Deep sleep enters a different
system mode, and there the radio does let go.

For contrast, `cpu_sleep` - which hands the radio back entirely - floors at 1.28 mA, and there
the core really is asleep.

**So a connected device should not expect anything from light sleep; use deep sleep.** That setting
is for devices that never join a network, and for platforms whose deep sleep returns through reset.

**The network matters more than every configuration option put together.** The same binary draws
several times more on a busy network than on a quiet one. The difference is all radio receive: the
trace shows a wide burst at receive-level current every couple of seconds whenever there is traffic
about. **Always state the network conditions with a low power number**, or it cannot be compared to
anything.

- **`none`** is the baseline. Measure it first: the step to either option below is what the cpu was
  costing, and without it the total cannot be attributed.
- **`light`** is the portable choice. Memory, peripherals and the association all survive, and any
  interrupt - the beacon included - brings the core straight back. Every platform means the same
  thing by it.
- **`deep`** goes further, but *further* differs by platform. Where it resumes in place
  (GD32VW553) it can stop the pll and the crystal as well, which is the bulk of the remainder -
  GigaDevice document 1.43 mA for this combination in AN150. Where it instead saves state and
  returns through reset (T5AI), the association does not survive, which defeats a connected
  device. Check which one your platform does first.

Fill in the access point before running:

```bash
tos.py config set EXAMPLE_WIFI_SSID=myrouter EXAMPLE_WIFI_PASSWORD=mypassword
```

## Expected output

```
connecting to "Pico" ...
connected
wifi power save on, dtim 1 -> 0
cpu light sleep   -> 0
awake
awake
```

- `-> 0` means power save was enabled; a negative value means the platform has no implementation
- A failure to connect exits with a reason rather than enabling power save on a dead association,
  which would look like power save itself failing
- `awake` every 5 s only separates a dozing device from a hung one

## How to verify

**Use a current meter.** The log cannot show whether the radio is sleeping - a device with its
receiver on the whole time prints exactly the same thing.

**The shape of the trace says more than the average.** Done right it looks like this:

- **a floor at practically zero** - deep sleep really went down, pll and crystal included
- **regular narrow spikes** - one wakeup per DTIM to collect the beacon, spaced `DTIM x 102.4 ms`
- **occasional wide bursts** - something on the network to receive, at receive-level current
  (around 130 mA on this part)

The average is those three weighted by time, and **how much of it is wide bursts depends on how
noisy the network is, not on your code**. A floor that is not near zero is the part that is yours.

Put a meter in the supply and use `EXAMPLE_WIFI_PS` for the comparison:

```bash
tos.py config set EXAMPLE_WIFI_PS=n     # measure with it off
tos.py config set EXAMPLE_WIFI_PS=y     # and with it on
```

The step between them is what the feature is worth. Then walk `EXAMPLE_WIFI_PS_DTIM` from 1 to 5 to
10 and watch the current fall - that curve is the most useful thing this example produces.

**With it off the example asks for power save to be off rather than skipping the call.** Platforms
commonly switch it on themselves once the interface is a station - GD32VW553 does - so skipping
would measure the same thing twice.

The beacon rhythm is visible in the trace as well: with power save on the current is a regular
train of spikes, one per wakeup, spaced roughly `DTIM interval x 102.4 ms` apart.

## Together with cpu sleep

The two add up rather than compete:

| | saves |
|---|---|
| `wifi_ps` | the radio receiver |
| `cpu_sleep` / `cpu_deep_sleep` | the cpu core and its clocks |

Once the radio is dozing the cpu has nothing to do between beacons either, so it can go down with
it. A real low power product wants both.
