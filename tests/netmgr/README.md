# netmgr_retry host unit test

A host-side (no target, no build tree) unit test for
`src/tuya_cloud_service/netmgr/policy/netmgr_retry.c`.

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

8. **`__netmgr_retry_reached()`'s clamp bound** -- three checks, not one:
   `netmgr_retry_interval_ms()` of a table entry of `UINT32_MAX` seconds
   must answer an interval under `2^31` ms (the invariant the idiom needs,
   not the exact clamped value); armed against that same table at
   `now_ms = 0` and polled at `now_ms = 2,000,000,000`,
   `netmgr_retry_due()` must be `FALSE` and `netmgr_retry_remain_ms()` must
   be nonzero; and `netmgr_retry_fail()`'s armed deadline, measured from
   the arming instant, must itself stay under `2^31` ms. See "Fixed: a
   false-positive due() on an oversized table entry" below for what these
   guard against.

132 assertions in total (exact count printed at the end of a run).

### Probed but not asserted

Two behaviours the header does not document are exercised and printed, but
never asserted, so the test cannot accidentally freeze "what the code
happens to do today" into a contract nobody actually wrote down:

- **`netmgr_retry_advance()` with a `NULL` `attempt` pointer.** The header's
  `@param` block for `advance()` never says this parameter can be `NULL`;
  only the `.c` file's comment does. Observed: it does not crash, and
  returns `netmgr_retry_interval_ms(table, 0)` — i.e. the attempt-0 interval
  — without touching any counter.
- **`uint32_t` millisecond-base arithmetic at the ordinary wrap point**
  (a deadline armed just before the counter rolls over at `0xFFFFFFFF`,
  `now_ms` just after). Probe B confirms `due()` and `remain_ms()` agree
  with reality there.

A third probe used to live here: an oversized caller-supplied table entry,
exercised and printed but never asserted, reported as "a finding" because
it exposed a real bug rather than an absence of documentation. It is not a
probe anymore -- section 8 above asserts the bound it was measuring, and
"Fixed: a false-positive `due()` on an oversized table entry" below covers
what was wrong and what now guards it.

## Fixed: a false-positive `due()` on an oversized table entry

`__netmgr_retry_reached()` in `netmgr_retry.c` compares deadlines with a
signed-difference idiom specifically so it survives the ordinary uint32_t
millisecond wrap (the counter rolls over every ~49.7 days). Its own comment
used to state the idiom needs the interval to stay "far below 2^31 ms
(24.8 days)" and then argued the bound held because "the longest interval
this module can produce is 600s ... and even a clamped
`NETMGR_RETRY_INTERVAL_MAX_S` entry stays under 2^32 ms, so the only way to
break the assumption is a deadline armed more than 24.8 days in the future,
which no table here can express."

That conclusion did not follow from the arithmetic it was built on. The old
`NETMGR_RETRY_INTERVAL_MAX_S` was `0xFFFFFFFF / 1000 = 4294967` seconds --
clamped there only so the `seconds * 1000` multiplication would not
silently overflow -- and 4294967 seconds is ~49.7 days: comfortably under
2^32 ms, but nearly **double** the 2^31 ms / 24.8-day bound the comment
itself required. The clamp named as the reason the bound could not be
broken was the very thing breaking it, because `netmgr_retry.h`'s own text
on `NETCONN_CMD_RECONN_TABLE` says the entry *count* is clamped, not the
entry *values* ("a nonsense entry can reach a table"), so a
product-supplied table -- the only route into this module for
caller-chosen values; the two built-in tables never exceed 600s -- could
legally carry one.

Reproduced: armed at `now_ms = 0` against a table with one entry of
`NETMGR_RETRY_INTERVAL_MAX_S` seconds (the old value, 4294967s), the
deadline lands at `4294967000` ms. Polled at `now_ms = 2000000000`
(~23.1 days after arming; the real deadline was still ~26.6 days out),
`netmgr_retry_due()` answered `TRUE` and `netmgr_retry_remain_ms()`
answered `0` -- a false-positive fire more than three weeks early, from the
signed-difference compare overflowing `int32_t` once the gap between
`now_ms` and `deadline_ms` exceeded `2^31 - 1` ms.

The fix is at the clamp, not at the comparison: `NETMGR_RETRY_INTERVAL_MAX_S`
is now `0x7FFFFFFF / 1000 = 2147483` seconds, so a clamped entry arms
`2147483000` ms -- `648` ms inside the `2^31` ms (`2147483648` ms) limit the
idiom actually needs. The comment above `__netmgr_retry_reached()` now
states that bound on both sides instead of the disproved one: an interval
armed above `2^31` ms reads as already due before it is, and a deadline
left unpolled for `2^31` ms (24.8 days) *after* it genuinely passed reads
as not-yet-due again -- the same truncation, on the other side of zero.
`NETMGR_RETRY_INTERVAL_MAX_S` enforces the first half for every table this
module can be handed; the reason it holds is no longer "no table here can
express that value" but "the clamp does not let one through."

Section 8's three assertions are the regression: a `UINT32_MAX`-second
entry must yield an interval under `2^31` ms; the exact repro above
(`due()` at `now_ms = 2000000000` after arming at `0`) must answer `FALSE`,
with a nonzero `remain_ms()`; and `netmgr_retry_fail()`'s armed deadline
must itself stay under `2^31` ms past the arming instant. Reverting the
clamp to `0xFFFFFFFF / 1000` turns all three of those checks red -- the
first thing this harness has paid for: a proof that used to live only in a
comment, and was wrong, now goes red when it breaks again.

Contrast with the *ordinary* wrap case (deadline armed just before the
uint32_t counter rolls over, `now_ms` just after) using a realistic interval
from either shipped table (max 600000ms): that case was always handled
correctly -- probe B in the test confirms `due()` and `remain_ms()` agree
with reality there. The bug was specifically about the *magnitude* of the
gap between `now_ms` and `deadline_ms`, not about crossing the wrap point
per se, and it was only reachable through a caller-supplied table with an
entry close to the old `NETMGR_RETRY_INTERVAL_MAX_S` -- not through
`netmgr_retry_table_assoc` or `netmgr_retry_table_revalidate`, and not
through long-running use of either of those. Latent, not live -- and now
fixed at the clamp instead of left as a comment asserting it could not
happen.

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
