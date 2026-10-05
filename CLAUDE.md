# CURIO ISLES — instructions for Claude Code

Read this file in full before changing anything. It is the standing spec for the project; the original pitch
is summarised here with the decisions taken when it was turned into an Unreal project.

## What this is

A puzzle adventure across an archipelago of science islands: fix wonderful broken machines using real
science. Every level is a broken contraption, a tray of parts (ramps, launchers, brake pads...) with sliders
for real quantities, and a PLAY button. Core loop: **Build → (Predict) → Play → Learn**. Island 1 is
**Newton's Island (Physics)**, the launch game; more islands (Chemistry, Biology, Astronomy, Maths) come
later as content packs in the same app. Landscape, 1–3 minute levels, audience 13+ (students and casual
players). Publisher Brainrot Interactive Studios, app id **`com.brainrotinteractive.curioisles`** (permanent
after the first Play upload). Tagline: *Break it. Fix it. Understand it.*

**Engine decision (2026-09-30):** the pitch said Unity 6 / C#; the user chose **Unreal Engine 5.8, C++**,
built the way EMBERHOME was (`C:\SACHIN\Hollowlight\PLAYBOOK.md`). Unity terms in the pitch map to:
ScriptableObject/JSON levels → JSON files under `Content/Islands/`; Addressables island packs → one folder
per island (downloadable packs via Play Asset Delivery later); in-Unity level editor → an in-game editor in
development builds (M2); URP 2D lights / Spine → procedural canvas drawing (below).

## Non-negotiable design rules (from the pitch)

- **Two modes, one game.** Fun Mode (numbers hidden, sliders say "less ↔ more", one-line "why it worked")
  and Student Mode (real values with units, metre grid, measurements, velocity arrows, formula card with a
  worked example from the current values, syllabus tags). **The mode changes only the UI, never the
  physics** — the simulation never sees it (test `CurioIsles.Game.PlayMatchesCoreInBothModes`).
- **Failure is fun, not punishing.** No lives, no game-over screen: funny failures (splash, sink), Professor
  Newton sighs, RETRY keeps the parts where they were.
- **Stars:** solved · used par parts or fewer · correct prediction (predict arrives in M2; the third star
  is shown as "predict (soon)").
- **Monetisation (M4):** World 1 free + first level of every world; one-time island unlock; optional
  rewarded ads only; no energy, no loot boxes, no pay-to-win.

## Architecture

```
Private/Core    engine-agnostic C++ (no Unreal headers): CIMath (deterministic maths), CIJson, CIContent
                (islands/worlds/parts/levels from JSON), CIParts (part geometry), CISim (the physics),
                CISolver (proves levels solvable), CIExpr (formula templates), CIChecks (shared tests)
Private/Game    Unreal wiring: ACIGameMode (no pawn), ACIPlayerController (pointer/touch/keys, lifecycle,
                capture script, console), ACIHUD, FCIGame (screens + level controller), UCISaveGame,
                CIContentLoader (reads Content/Islands)
Private/Render  FCIDraw (batched canvas triangles + text), FCIWorldRenderer (backdrop, level, parts, effects)
Private/UI      FCIUI (title, island map, top bar, tray + slider strip, buttons, result card, fail bubble)
Private/Tests   automation tests
Content/Islands/<Island>/island.json, parts.json, levels/*.json   all game content
Tools/SimHarness  builds Core with plain MSVC: every physics/level check in ~2 s
```

- **Core must never include Unreal headers.** It is also compiled by `Tools/SimHarness`.
- **Deterministic physics (critical).** Predictions and correct answers must give the same result on every
  run and every device. The sim (`CISim`) uses fixed 1/120 s steps; between events a body moves with
  constant acceleration integrated exactly (x += vt + ½at²), so free flight, slopes and braking match the
  textbook formulas to rounding; events (hitting a surface, crossing a brake edge, stopping) are found by
  48-step bisection inside the step. Only +, −, ×, ÷, sqrt and our own Sin/Cos (`CIMath`) — never library
  transcendental functions, never FMA: the module builds with `FPSemantics = Precise` (UE's Windows game
  default is `/fp:fast`) and Core files add `fp contract(off)` pragmas. The sim is copyable by value (the
  dotted path preview and the solver clone it) and never allocates while stepping.
- **Unity-physics equivalent for decoration only:** confetti, dust and splashes are `FCIParticle`s in the
  game layer, never part of the simulation.
- **Data-driven.** Levels, parts, worlds and islands are JSON. A part's *behaviour* is code
  (`EPartBehavior`: Ramp, Launcher, Brake); its sliders, ranges, words, look and constants are data. A new
  world or island is new JSON (plus art/behaviours only when it needs new kinds of parts).
- **Track joints:** a body sliding along a surface onto an adjoining one within 60° follows it at the same
  speed (a smoothly curved track), so energy is conserved as in the textbook; steeper corners are walls.
- **No authored assets.** The game runs on `/Engine/Maps/Entry` with world rendering disabled; `ACIHUD`
  paints everything with canvas triangles and the engine's Roboto font. The look (illustrated storybook:
  soft rounded shapes, warm gradients, layered parallax hills with haze, grey veil over the broken machine
  that washes away in a colour wave on success) lives in code. Painted art can be layered in later
  (`FCIDraw::ScreenTexture`-style sprites) without changing the architecture.
- Every class/file gets a one-line comment pointing back to the CLAUDE.md section it belongs to.

## Content format (`Content/Islands/<Island>/`)

- `island.json`: id, name, subject, mentor, `parts` file, `worlds` [{id, number, name, teaches, theme
  colours, levels [files]}].
- `parts.json`: parts [{id, name, blurb, behavior, look, params [{id, label, symbol, unit, min, max, step,
  default, decimals, less, more}], props {behaviour constants}}]. Sliders snap to `step`.
- Level: id (`newton.w1.08`), name, goal, concept, syllabus [], why (Fun), whyStudent, formula, example
  (template), stopText, timeLimit, par, previewSeconds, view [x0,y0,x1,y1] m, surfaces [{points, friction,
  restitution, look: ground|block|track|basket}], zones [{id, look, rect, level}], bodies [{id, kind
  ball|cart, radius, pos, vel, on {part, anchor}}], slots [{id, pos, accepts, facing}], tray [{part, count,
  params overrides}], fixed [{part, slot, tunable, params}], goals [{type enter|rest, body, zone}], fails
  [{body, zone, say}], predict {question, measure, choices}.
- Templates: `{height}` / `{ramp.height:2}` slider values, `{g}`, sim measures `{ball.exitSpeed}`,
  `{cart.x}`, `{ball.landX}`...; `{=expr:1}` evaluates maths (`sqrt`, `sin` in degrees, `^`). Unknown names
  render `[?]` and fail the `TextsRender` check.
- JSON allows `//` comments and trailing commas. Units: metres, seconds, y up.

## Levels (M1 set)

| Id | Name | Teaches | Solver: winning setups |
|---|---|---|---|
| newton.w1.01 | First Roll | speed from height (v = √2gh) | 50% (discover level) |
| newton.w1.08 | Brake! | deceleration (s = u²/2a) | 4.1% |
| newton.w3.03 | Canyon Shot | projectile range (R = v² sin2θ / g) | 6.1% |

**Every level must be solvable within par by the solver, must not be solved by untouched sliders, and must
be bit-identical over 100 runs.** After any level or physics change run the harness (seconds), then the
automation tests:

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 [-Verbose] [-Level w1.08 [-Trace]] [-Fast]
```

## Build, test, run (Windows, UE 5.8 launcher build)

Engine root `C:\Program Files\Epic Games\UE_5.8` (override with `UE_ROOT`).

```
# Regenerate project files (after adding/removing .cpp files)
"<Engine>\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<repo>\CurioIsles.uproject" -game -rocket
# Compile the editor target (close the editor first: Live Coding blocks outside builds)
"<Engine>\Engine\Build\BatchFiles\Build.bat" CurioIslesEditor Win64 Development -project="<repo>\CurioIsles.uproject" -waitmutex
# All automation tests, with a pass/fail summary
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1 [-NoBuild]
# Visual check: every screen and level state in Fun and Student Mode -> Saved/Screenshots/WindowsEditor/
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1 [-Phone]
# Package (Packaged/<Platform>/)
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64|Android|IOS [-Release]
```

Command line: `-CILevel=<world>.<level>` (0-based) starts a level, `-CICapture [-CICaptureTag=x]` runs the
capture script and quits. Console: `ci.Play <w> <l>`, `ci.Solve` (the solver's answer), `ci.Mode
fun|student`, `ci.HideUI 0|1`, `ci.ResetProgress`. Keys: Space/Enter = PLAY / next / retry, R = retry,
P = pause, Tab or M = toggle mode, Esc = back.

The harness exe is named `cisim_harness.exe` and built with the debug CRT (heap statistics for the
no-allocation check). Do not replace global `operator new` in it: the antivirus on this PC quarantines the
resulting exe.

`DisableEnginePluginsByDefault` is on in the .uproject: engine plugins are opt-in. Add one only if the game
needs it at runtime, and mark platform-specific ones `Optional`.

## Milestones (from the pitch)

- [x] **M1 — Core prototype.** Deterministic sim core (projectile + ramp + cart/brake) with textbook
  tests; drag-and-drop parts with snapping, invalid spots in red, removal; sliders; Play / pause / 0.25x /
  Retry; success animation + "why it worked"; Fun/Student toggle; 3 test levels.
  - [x] Same inputs → same result, 100 runs bit-identical (harness + automation test).
  - [x] Projectile range = v² sin2θ / g within 1% for 10 cases (exact to 4 decimals).
  - [x] Zero allocations in the sim loop (harness heap statistics).
  - [ ] 60 FPS on a mid-range Android phone — needs an APK on a real device (see SETUP.md).
- [ ] **M2 — Vertical slice.** World 1 Motion Meadow (12 levels) at final art quality, Predict system (the
  third star), Lab Notebook screen, in-game level editor, hints (nudge → ghost part → solver replay),
  audio (procedural `USynthComponent`, as in EMBERHOME), Fredoka font.
- [ ] **M3 — Content.** Worlds 2–3, island map, archipelago map with locked islands, save polish, audio.
- [ ] **M4 — Launch.** IAP unlock, analytics, store assets, trailer, Play release with Worlds 1–3.

## Platforms

- **Windows**: editor builds and runs (`capture.ps1`).
- **Android**: configured (`com.brainrotinteractive.curioisles`, SensorLandscape, arm64, Vulkan + ES3.1, min
  SDK 26, target 36, MaxAspectRatio 3.0, display cutout on, no AdMob, no billing until M4). Development
  package builds (2026-09-30: `CurioIsles_universal.apk`, ~100 MB); not yet run on a device.
- **iOS**: configured (bundle id as above, landscape, Metal). Needs a Mac + Xcode + Apple account.
- Touch: one pointer (first finger), taps click on release, generous hit zones; the app pauses and saves
  when sent to the background. Safe-area insets from `FDisplayMetrics::TitleSafePaddingSize`.

## Controls (user decision 2026-10-05: "like Angry Birds", no select-then-slider)

- Parts are tuned **directly on the machine** by their grips (`FCIGame::Grips`, `DragGrip`): pull the
  launcher's ball back and let go to fire (angle = opposite the pull, speed = pull length, the first 0.7 s of
  the real flight shown as dots while aiming; release runs the level); lift or lower the ramp by the grip
  above its flag; slide the knob along the brake pad. Values still snap to slider steps, so the solver and
  the physics are unchanged. There is no selected part and no slider strip (the strip code in `CIUI.cpp`
  is kept only for tunable fixed parts).
- Placing is unchanged: drag from the tray onto a glowing spot (or tap the card); drag a part back to the
  tray to remove it. PLAY remains for levels without a launcher.
- Look: sun rays, per-world scenery (meadow windmill and trees; canyon mesas and hot-air balloons), contact
  shadows, a motion streak behind the ball, vignette, lighter veil over the unsolved machine.
