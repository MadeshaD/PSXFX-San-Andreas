# 0.29 startup UI

The hook at 0x619D2E forwards to RwEngineStart at 0x7F2E70 and preserves its return value. On success with a device available, the hook records the render thread and attaches existing D3D9 and Im2D handlers. The original call and test/branch bytes are checked before patching. A modified site keeps the original scene attachment path. This build requires the usual early ASI Loader initialization.

CPU/UI regressions and static x86 verification are available under tests. Windows startup and visual results require an in-game check.

## Mod Loader 0.31

Mod Loader startup forwards through the current CRT WinMain call destination after scanning ASIs. The ASI restores its own operand before initialization and preserves the live destination, stdcall arguments and result. The original RwEngineOpen bootstrap is a fallback if the CRT site cannot be armed. Verify actual installation using Logging=1 near the ASI.
