# The Saga Cutter

A Viking-themed colony sim made for a game jam. Unreal Engine **5.9**, C++ module + Blueprints.

## Layout

```
.mcp.json, Scripts/mcp/   Claude <-> Unreal Editor MCP tooling (see Docs/UE_MCP.md)
Docs/                     notes
UE/                       the Unreal project  (open UE/TheSagaCutter.uproject)
  Source/TheSagaCutter/   C++: game mode, RTS camera, controller, colonist, building site, gameplay tags
  Config/                 ini files (MCP server settings live here)
  Content/Vikings/        game content: Maps, Characters, Buildings, Environment, UI, Data, Blueprints
```

## Getting started

1. Open `UE/TheSagaCutter.uproject` with UE 5.9 (right-click -> Generate Visual Studio project files for C++ work).
2. Build `TheSagaCutterEditor | Win64 | Development` (VS 2022) the first time; later changes can use Live Coding.

## Claude + MCP

The MCP server is enabled in the project (`ModelContextProtocol` + a curated set of toolsets). Claude Code picks it up through
`.mcp.json`; the editor is started on demand with `ue_start`. Details and gotchas: [Docs/UE_MCP.md](Docs/UE_MCP.md).
