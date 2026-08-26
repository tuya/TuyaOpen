# netmgr_retry host unit test

A host-side (no target, no build tree) unit test for
`src/tuya_cloud_service/netmgr/netmgr_retry.c`.

## Why this exists

Commit `2eae2654` said the back-off's "properties are written down and
tested" and that the observable sequence — 1, 3, 5, 10, 15, 20, then 20
forever — was "verified against the old code across the whole attempt
range." Commit `c97601a5` then recorded that `tests/` had no netmgr test to
point at (only `tests/export/`'s shell tests existed), so those numbers came
from a subagent report that got paraphrased straight into the commit message
with nothing to check them against. `c97601a5`'s conclusion was "adding one
is the honest fix." This is that test.

## Why `netmgr_retry` and not some other netmgr file

It is not an arbitrary starting point. Of everything in
`src/tuya_cloud_service/netmgr/`, `netmgr_retry.c` is the one file whose
`#include` list is nothing but its own header, and `netmgr_retry.h`'s
`#include` list is nothing but `tuya_cloud_types.h`. Its seven exported
functions are pure arithmetic over a caller-owned `netmgr_retry_t` — no
globals, no allocation, no OS calls, no timer, no mutex (the header says so
explicitly: "no timer, no thread, no global state"). And its header already
documents every contract precisely enough to transcribe into assertions
instead of inventing them, which is exactly what this test does.

## Run it

```bash
bash tests/netmgr/run_all.sh
```

Needs a C compiler (`cc`/`gcc`) on `PATH` — nothing else. It compiles
`netmgr_retry.c` straight from `src/`, links it against
`test_netmgr_retry.c`, runs the resulting binary out of a `mktemp -d`
directory, and deletes that directory afterwards. Runs in well under a
second (typically ~0.3–0.8s including the compile). No `.o` or binary is
left in the repo.

## What is covered

`test_netmgr_retry.c` transcribes `netmgr_retry.h`'s contract, section by
section:

1. **The two tables** — `netmgr_retry_table_assoc` is `{1,3,5,10,15,20}`,
   count 6; `netmgr_retry_table_revalidate` is `{30,60,120,300,600}`, count
   5.
2. **`netmgr_retry_interval_ms()`** — the sequence `2eae2654` named:
   attempts 0..5 on the assoc table give 1000/3000/5000/10000/15000/20000ms,
   attempts 6, 7 and 99 all clamp to 20000ms. Same shape for the revalidate
   table (30000..600000ms, then constant). Plus the "deliberately total"
   contract: `NULL` table → 0, a `count == 0` table → 0, and a table with a
   `NULL` `entry` but nonzero `count` → 0 (the last one is documented in the
   `.c`'s `__netmgr_retry_count()` comment, not the header, but it is stated
   explicitly rather than being an accident).
3. **`netmgr_retry_advance()`** — returns the pre-bump interval; the
   attempt counter saturates at `count - 1` and does **not** wrap even after
   30 consecutive calls (the header explains why saturation matters: a
   wrapping counter would restart the back-off at one second every sixteen
   failures). `NULL` table and an explicit `count == 0` table both leave the
   counter at 0 forever and return 0.
4. **`netmgr_retry_bind()` / `netmgr_retry_reset()`** — `NULL` is a no-op
   for both (asserted by not crashing); `bind()` copies the table and zeroes
   attempt/deadline; `bind(ctx, NULL)` leaves the context with no table;
   `reset()` after a real failure brings attempt back to 0 and deadline back
   to unarmed.
5. **`netmgr_retry_fail()`** — `NULL` retry and an empty table both return
   0, but an **empty table is still armed at `now_ms`**, not left unarmed —
   `netmgr_retry_due()` keeps answering `TRUE` forever afterward, which is
   the behaviour the association consumer depends on (immediate retry) and
   the revalidation consumer must avoid triggering (by not calling `fail()`
   at all on a count-0 table). A seven-step sequence of continuous failures
   against the assoc table checks the returned deadlines end to end:
   1000 → 2000 → 5000 → 10000 → 20000 → 35000 → 55000 → 75000.
6. **`netmgr_retry_due()`** — `NULL` → `FALSE`; unarmed → `FALSE`; armed and
   exactly at the deadline → `TRUE`; one ms before → `FALSE`.
7. **`netmgr_retry_remain_ms()`** — the three-way contract from the header,
   asserted as three separate cases: not armed → 0; armed and due → 0;
   armed and future → the actual remainder, **never** 0, including at the
   1ms boundary.

128 assertions in total (exact count printed at the end of a run).

### Probed but not asserted

Two behaviours the header does not document are exercised and printed, but
never asserted, so the test cannot accidentally freeze "what the code
happens to do today" into a contract nobody actually wrote down:

- **`netmgr_retry_advance()` with a `NULL` `attempt` pointer.** The header's
  `@param` block for `advance()` never says this parameter can be `NULL`;
  only the `.c` file's comment does. Observed: it does not crash, and
  returns `netmgr_retry_interval_ms(table, 0)` — i.e. the attempt-0 interval
  — without touching any counter.
- **`uint32_t` millisecond-base arithmetic**, both at the ordinary wrap
  point and with an oversized table entry. See "A finding" below — this one
  turned up a real correctness question, not just an absence of
  documentation.

## A finding: `netmgr_retry_due()` / `remain_ms()` can misfire on an
oversized table entry

`__netmgr_retry_reached()` in `netmgr_retry.c` compares deadlines with a
signed-difference idiom specifically so it survives the ordinary uint32_t
millisecond wrap (the counter rolls over every ~49.7 days). Its own comment
states the idiom needs the interval to stay "far below 2^31 ms (24.8 days)"
and then claims that bound holds because "the longest interval this module
can produce is 600s ... and even a clamped `NETMGR_RETRY_INTERVAL_MAX_S`
entry stays under 2^32 ms, so the only way to break the assumption is a
deadline armed more than 24.8 days in the future, which no table here can
express."

That last clause does not follow from the arithmetic. `NETMGR_RETRY_INTERVAL_MAX_S`
is `0xFFFFFFFF / 1000 = 4294967` seconds — clamped there only so the
`seconds * 1000` multiplication does not silently overflow — and 4294967
seconds is **~49.7 days**, which is comfortably under 2^32ms but nearly
**double** the 2^31ms/24.8-day bound the wrap-safety comment itself
requires. `netmgr_retry.h`'s own text on `NETCONN_CMD_RECONN_TABLE` says
the entry *count* is clamped, not the entry *values* ("a nonsense entry can
reach a table"), so a product-supplied table (the only route into this
module for caller-chosen values; the two built-in tables never exceed 600s)
can legally carry such an entry.

`tests/netmgr/test_netmgr_retry.c`'s probe C reproduces it directly:

```c
static const uint32_t huge_entry[] = {4294967u}; /* NETMGR_RETRY_INTERVAL_MAX_S */
netmgr_retry_table_t huge_table = {huge_entry, 1};

netmgr_retry_t ctx;
netmgr_retry_bind(&ctx, &huge_table);
uint32_t deadline = netmgr_retry_fail(&ctx, 0);       /* deadline_ms = 4294967000 */

netmgr_retry_due(&ctx, 2000000000u);                  /* -> TRUE  */
netmgr_retry_remain_ms(&ctx, 2000000000u);             /* -> 0     */
```

Real elapsed time since arming is 2,000,000,000ms (~23.1 days); the real
remaining time to the deadline is ~2,294,967,000ms (~26.6 days). Both calls
should answer "not due yet" / "~26.6 days remaining." Instead `due()`
reports `TRUE` and `remain_ms()` reports `0` — a false-positive fire more
than three weeks early, purely from the signed-difference compare
overflowing `int32_t` once the gap between `now_ms` and `deadline_ms`
(remaining *or* elapsed) exceeds `2^31 - 1` ms.

Contrast with the *ordinary* wrap case (deadline armed just before the
uint32_t counter rolls over, `now_ms` just after) using a realistic interval
from either shipped table (max 600000ms): that case is handled correctly —
probe B in the test confirms `due()` and `remain_ms()` agree with reality
there. The bug is specifically about the *magnitude* of the gap between
`now_ms` and `deadline_ms`, not about crossing the wrap point per se, and it
is only reachable through a caller-supplied table with an entry close to
`NETMGR_RETRY_INTERVAL_MAX_S` — not through `netmgr_retry_table_assoc` or
`netmgr_retry_table_revalidate`, and not through long-running use of either
of those. This test does not assert on it (asserting a bug's current
behaviour would document it as intended), does not attempt a fix, and does
not touch `src/`; it is reported here as a finding for someone who owns that
code to decide what to do with, per the instructions this test was written
under.

## The type shim — the one assumption this whole approach rests on

`netmgr_retry.h` includes only `tuya_cloud_types.h`, and that header pulls
in `tuya_iot_config.h`, which in turn pulls in the build-generated
`tuya_kconfig.h`. Compiling against the real header would make this test
depend on a full `tos.py build` having already run for some platform —
which is the surest way to end up with a test nobody runs. So
`tests/netmgr/shim/tuya_cloud_types.h` defines *only* what
`netmgr_retry.h`/`netmgr_retry.c` actually use — `uint32_t`/`int32_t` (from
`<stdint.h>`), `NULL` (from `<stddef.h>`), and `BOOL_T`/`TRUE`/`FALSE` — and
`run_all.sh` puts `tests/netmgr/shim` on the include path ahead of anywhere
a real `tuya_cloud_types.h` might resolve (in fact `run_all.sh` never adds
any such path at all, so the real header is unreachable, not just
shadowed).

**This is checked, not assumed.** Every `tuya_cloud_types.h` in the tree was
grepped directly:

```
platform/LN882H/.../tuya_cloud_types.h   md5 04296bc7ed640268eb293f69f26fb92f
platform/LINUX/.../tuya_cloud_types.h    md5 236acd8783babd4f9ec35fc1de5fadf1
platform/T2/.../tuya_cloud_types.h       md5 bf68efa4e9f7af11485929c063b5c219
platform/T5AI/.../tuya_cloud_types.h     md5 1b7ed039f92f12b765aca1af3a47f098
platform/BK7231X/.../tuya_cloud_types.h  md5 bf68efa4e9f7af11485929c063b5c219
platform/T3/.../tuya_cloud_types.h       md5 8e67588e048a42cb1f2b9768e9573e4b
platform/ESP32/.../tuya_cloud_types.h    md5 bf68efa4e9f7af11485929c063b5c219
tools/porting/adapter/.../tuya_cloud_types.h  md5 f6c0e405f91437c73d16724e657a4562
```

Eight files, five distinct contents — and every one of them agrees on:

```c
typedef int BOOL_T;
...
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
```

which is exactly what the shim defines. `uint32_t`/`int32_t` come from
`<stdint.h>` in every real header too (via `#include <stdint.h>` near the
top, alongside `<stddef.h>`), so the shim's use of the same standard headers
is not a divergence, just a shortcut past `tuya_iot_config.h`.

**Where this could stop holding.** If a future `tuya_cloud_types.h`
redefines `BOOL_T` to something other than `int` (an `enum`, for instance,
or a fixed-width type), or changes what `TRUE`/`FALSE` expand to, the shim
would silently diverge from reality and this test would keep passing while
testing a slightly different ABI than the one `netmgr_retry.c` actually
gets compiled against in a real build. Nothing currently guards against
that drift automatically — the check above is a point-in-time grep across
the tree, not a build-time assertion. If `netmgr_retry.h` ever grows an
`#include` beyond `tuya_cloud_types.h`, or starts using a type this shim
does not define, `run_all.sh`'s compile step will fail loudly (undeclared
identifier / no such file), which is the cheapest failure mode available and
the reason the shim stays this minimal rather than trying to be a more
complete stand-in.

## Layout

| File | Purpose |
|------|---------|
| `run_all.sh` | Compiles and runs the test; exit code is the failure count reduced to 0/1 |
| `test_netmgr_retry.c` | The assertions themselves, plus the informational probes |
| `shim/tuya_cloud_types.h` | Minimal type shim — see above |
