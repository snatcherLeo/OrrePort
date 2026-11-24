# Credits

OrrePort is built on other people's decompilation work. The licence terms are in `LICENSE` and `NOTICE` (both
unchanged); this file says who did the work the project builds on.

## The decompilation

- **dougchansan**, [pkmn-colosseum](https://github.com/dougchansan/pkmn-colosseum): the Pokémon Colosseum (GC6E01)
  decompilation this repository was cloned from (MIT). Its build pipeline, its matched C and the names it gave the
  game's functions and data are the foundation of everything here.
- **Count-Sheep**, [pkmn-colosseum fork](https://github.com/Count-Sheep/pkmn-colosseum): the fork that took the
  decompilation to every function matching the original game, through commit `51bfe4314`. That matched C was merged
  into OrrePort in October 2026, with OrrePort's PC-port fixes re-applied on top of it. The local-campaign harness it
  used lives in its own repository (`Pokemon-Decomp-Harness`).

## References

- **doldecomp/melee**: the HSD / sysdolphin reference used for the shared engine library (see `NOTICE` for the exact
  snapshot).

## Libraries OrrePort ships

- **Monocypher** 4.0.3 by Loup Vaillant, Michael Savage and Fabio Scotoni
  ([monocypher.org](https://monocypher.org/)), BSD-2-Clause or CC-0: the Ed25519 signature check that makes sure an
  update really comes from OrrePort, in both OrrePort and OrrePort Studio. Its licence ships as
  `app\licenses\Monocypher-LICENCE.txt`.
- The other libraries' licences (RmlUi, FreeType, HarfBuzz, SheenBidi, libpng, libwebp, stb_image, libusb, libwdi)
  ship in `app\licenses\` the same way.

Where OrrePort's own work starts (the PC port, its fixes and the engine documentation) is recorded in the project's
history. OrrePort's About screen and release notes will carry these credits too.
