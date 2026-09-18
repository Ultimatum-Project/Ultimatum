# Web tap-walk runtime verification

## Reproduced failure

Verified 2026-09-15 UTC using the real fixture-enabled WASM engine and shipped
HTML world-tap handler in the in-app browser. The test origin is isolated from
the LAN adventure and no user save was changed.

On the pre-fix engine, an unobstructed three-square route succeeded, but a
route with a moving creature aborted after the first square: moves changed
from 1600 to 1601, and Asyncify reported that an async operation was already
in flight. The error stack passed through `EngineClient.tapWorld` and the
shipped `tapWorld` click handler. Creature animations, encounters, and other
yielding turn work expose this; it is not a pathfinding failure.

Route startup previously dispatched movement directly from a browser `ccall`,
and delayed steps dispatched directly from a browser timer. Both now enqueue
SDL actions and execute only on the owning engine loop. Requests and delayed
steps retain generation validation; rapid queued destinations use the latest
request, and Menu invalidates pending startup requests.
Native iOS behavior is unchanged: these changes are web-guarded.

## Passing coverage

All eight real-engine cases passed on the rebuilt engine with no uncaught
browser errors or rejected async operations:

1. Three-square route followed by another D-pad move.
2. Two rapid taps: only the latest queued destination executes.
3. Retargeting after the first square.
4. D-pad input cancels remaining route steps.
5. Menu cancels a pending delayed step; resuming permits movement.
6. Approach a door, open it, then move again.
7. Approach a real NPC, submit Goodbye, return to movement.
8. A real attacking creature starts combat after the first square; the combat
   Menu opens and resumes with active battle controls.

Fixture 36 provides clear grass terrain; 38 places an authored real NPC in an
isolated city; 39 places an actual engine door. Fixture 37 positions a real
attacker beside the first destination, so combat begins before random
wandering. Gameplay, animation, conversation, door, and combat processing are
the real engine paths; the harness does not emulate their rules.
The fixture builds and drivers are never included in the normal client.

Both normal Release and diagnostic fixture engines built successfully.
All 12 unit/contract tests and JavaScript syntax checks passed.
All 23 existing special-conversation runtime regressions also passed after
the fix. A separate normal Release-engine tab verified a reachable world tap
and responsive Menu afterward; the normal LAN server serves revision
`20260915r`. These checks did not save over the user's LAN adventure.

## Reproduce and limitations

```sh
cd clients/web
npm run test:runtime:build
npm run test:runtime:serve
```

Open `http://127.0.0.1:4174/?walk_suite=1` and inspect the visible report for
`SUITE PASS`. Add `&fixture=37` to isolate the first-step encounter.
Fixtures mutate only their isolated test adventure. Run these on the test
server, not the normal LAN server.

Physical Safari/Android verification, complete-adventure coverage, every
hazard/stale-target branch, and background interruption during a route remain
pending. This browser regression is evidence of the reported freeze being
fixed, not a claim of full mobile gameplay parity.
