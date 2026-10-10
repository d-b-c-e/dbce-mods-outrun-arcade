# Wheelkit fixtures

`cannonball-applied.xml` is, byte for byte, config.xml as Wheelkit's CannonBall adapter
(`Wheelkit.Core/Configure/CannonBallControlProfile.cs`) writes it for its synthetic profile:
`dbce-wheelkit` `tests/fixtures/configuration/cannonball-controls/expected/0.txt`.
`tests/input_inject_test.cpp` reads it with the game's own parser and `Config::load`'s key paths
(`Config::load_controls`) and drives the real input handlers with it.

When the adapter's output changes, copy the new expected file here in the same change.
