Port: LE adaptation by Mhacks79, GPLv3 with upstream linking exceptions.
Original NG source commit: 8228e9c4b70352b835012dd796e152a9f20ce187.
Original Mfg Fix reference commit: 270485ce10cbe20047b06abf27bb68553a454cff.
Original source notices and SDK licenses are retained verbatim.
In a VS2022 Developer PowerShell: cd Port; ./Build.ps1
To build the fake-engine test: ./Build.ps1 -Harness
Papyrus scripts are compiled with the LE Papyrus compiler and vanilla LE
script sources. The matching compiled scripts are in the package Scripts directory.
The native port does not link CommonLibSSE-NG or Detours.
