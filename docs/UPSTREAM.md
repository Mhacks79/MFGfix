# Upstream attribution

**MFG Fix NG LE** is an adaptation to Skyrim Legendary Edition by **Mhacks79**.

## Upstream projects

- **Mfg Fix NG — KrisV-777:** https://github.com/KrisV-777/Mfg-Fix-NG
  - Reference snapshot in the distributed archive: commit `8228e9c4b70352b835012dd796e152a9f20ce187`
- **Mfg Fix — andrelo1:** https://github.com/andrelo1/mfgfix
  - Reference snapshot in the distributed archive: commit `270485ce10cbe20047b06abf27bb68553a454cff`
- **Original Mfg Console — kapaer:** https://www.nexusmods.com/skyrim/mods/44596
- **Animation improvements — antpillager:** https://github.com/antpillager/mfgfix
- **SKSE Team:** Skyrim Script Extender, LE 1.7.3.

Original license files and source notices are retained in the supplied test archive.

## Licensing

The project source distribution includes **GNU GPLv3 (`COPYING`)** and the upstream **`EXCEPTIONS.md`** file describing modding/linking exceptions. Downstream distributors must comply with the relevant permissions and retain copyright and license notices.

## LE-specific scope

The LE port is not a literal port of every SSE engine edit. It instead adapts the expression/Papyrus interfaces to the 32-bit LE runtime, with a separate native implementation of the Mfg Console replacement.