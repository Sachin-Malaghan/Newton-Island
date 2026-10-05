# CURIO ISLES — store listing copy

Ready to paste into Google Play Console. Graphics are in `Build\Android\PlayStore\` (regenerate with
`Tools\Validation\capture.ps1 -Phone` then `Tools\Build\store_graphics.ps1`; icon from `make_icons.ps1`).

**App name (30 max):** Curio Isles: Science Puzzles
**Short description (80 max):** Fix wonderful broken machines with real physics. Break it. Fix it. Understand it.
**Category:** Games → Puzzle (tags: Physics, Educational, Brain games)
**Package name:** `com.brainrotinteractive.curioisles` (permanent after the first upload)

## Full description

Every machine on Newton's Island is broken. A ball that never reaches its basket. A cart that will not stop
before the pond. A bell across a canyon that nobody can ring. You can fix them — with real science.

Drop a part into the machine, pull it, tune it and let go. The ball flies, the cart brakes, the bell rings,
and the whole machine comes back to life in a burst of colour.

• Pull back and let go: aim the launcher like a slingshot and watch the curve
• Lift the ramp, slide the brake — every part is tuned right on the machine, no menus
• Real physics, the same every time: what works once always works
• Fun Mode: just play. Student Mode: real units, measurements on the machine, the formula and a worked
  example for every solve, with school-syllabus tags
• Funny failures, no punishment: no lives, no timers, no game over — tweak it and try again
• Collect a concept card in your Lab Notebook with every machine you fix
• No ads. No accounts. No tracking. Plays offline.

This is an early version with the first machines from Motion Meadow and Gravity Cliffs. More worlds are on
the way: Force Forest, Energy Waterworks, Fluid Lagoon, Wave Beach, Heat Volcano, Light Caves, Spark City
and Sky Station.

Break it. Fix it. Understand it.

## What's new (0.1.0)

First test version: three machines, Fun and Student Mode, pull-back-and-release controls.

## Answers for the review forms

- **Data safety:** no data collected, none shared. No account. Works offline.
- **Ads:** none. **In-app purchases:** none in this version (an island unlock is planned later; update
  this form, the privacy policy and `bSupportsInAppPurchasing` when it is added).
- **Content rating questionnaire:** no violence, no blood, no language, no gambling, no user-generated
  content, no chat. A cartoon cart can roll into a pond. Expected: PEGI 3 / ESRB Everyone.
- **Target audience:** 13 and over (13–15, 16–17, 18+). Do not tick under-13 age groups: that puts the app in
  the Families programme with stricter rules.
- **Permissions:** none.
- **Privacy policy URL:** `https://sachin-malaghan.github.io/Newton-Island/privacy.html`
  (live once GitHub Pages is enabled for the repo: Settings → Pages → Source: GitHub Actions. The repo
  must be public, or on a paid GitHub plan, for Pages to work.)
- **Support email:** your developer contact email.

## Upload steps (closed testing)

1. Once: `powershell -ExecutionPolicy Bypass -File Tools\Build\create_upload_key.ps1` (you type the
   password). Back up `Build\Android\curioisles-upload.keystore` and `Config\Android\AndroidEngine.ini`.
2. `powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release`
   → `Packaged\Android\CurioIsles-Android-Shipping.aab`.
3. Play Console → Create app → name above, Game, Free → fill in the forms from this file.
4. Testing → Closed testing → Create release → upload the `.aab` → add your tester list → send for review.
5. For every later upload raise `StoreVersion` (and `VersionDisplayName`) in `Config/DefaultEngine.ini`.
