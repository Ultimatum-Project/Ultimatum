## Platform architecture

The canonical current-state, target-contract, and migration specification is:

`docs/architecture/ULTIMATUM_PLATFORM_ARCHITECTURE.md`

Before planning or implementing shared platform extraction, a new game port,
catalog/library work, import/storage/save abstractions, engine-session contracts,
cloud sync, PWA/update behavior, or reusable native-host work, read the relevant
sections of that document first. Preserve its existing-data and `.u4save` v1
compatibility commitments unless the user explicitly approves a migration.

The implemented account/cloud baseline is documented in
`docs/engineering/ULTIMATUM_ACCOUNTS_CLOUD_SAVES.md`. Do not infer that cloud
sync is absent merely because a particular build or origin does not expose the
Account hub. The Supabase schema, migrations, web/iOS clients, immutable save
history, separately initiated private game-data upload, and shared 100 MiB
account quota are checked into this repository. Verify both backend migration
state and deployed frontend version before reporting availability.

## iOS UI and interaction design

The canonical mobile-port UI guidance for this repository is:

`~/Documents/PC_to_iOS_Interface_Playbook.md`

Before planning, designing, reviewing, or implementing any change involving:
- iOS UI or HUD layout
- touch controls or gestures
- mouse/keyboard-to-touch translation
- targeting or combat controls
- inventory, menus, windows, or overlays
- portrait/landscape behavior
- accessibility or controller support
- adaptation of desktop interactions for mobile

you MUST read the relevant sections of that playbook first.

Treat the playbook as the default design standard. Project-specific requirements may override it, but deviations should be intentional and called out in the implementation plan.

## iPhone delivery workflow

The user has explicitly requested automatic installation of new iOS builds on
their connected iPhone. After an iOS app change passes its relevant tests and a
signed device Release build succeeds:

- Inspect the installed bundle identified by `ZU4_IOS_BUNDLE_ID` in the ignored
  `.env.local`, and use the
  next integer as `ZU4_IOS_BUILD_NUMBER`.
- Build with `vendor/ultima4-ios/ios/build-local-device.sh` and the configured
  Apple development team.
- Install the signed app over the existing app with `xcrun devicectl` so its app
  data is preserved, then launch it.
- Verify the installed version and bundle version from the device and report
  the build number to the user.
- If the iPhone is unavailable, finish the verified code change and clearly
  report that automatic installation is pending.

This standing request covers local development-device installation and launch.
It does not authorize TestFlight or App Store uploads.

## Runtime verification

The user has requested autonomous runtime testing as part of finishing features.
Exercise changed gameplay flows in the real running engine and applicable UI,
including cancellation and return to main controls. Use isolated test adventures
so the user's saves and game data are preserved. Builds and static/unit checks
alone do not establish feature completion. Record the runtime evidence and any
remaining platform coverage honestly; do not ask again for routine in-scope
runtime browser testing. This does not authorize deployment or changes to the
user's real adventure.
