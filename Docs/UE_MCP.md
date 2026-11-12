# Unreal MCP in TheSagaCutter

Claude drives the Unreal Editor through Epic's built-in MCP server (plugin `ModelContextProtocol`, UE 5.9).

## How it is wired

```
Claude  --stdio-->  Scripts/mcp/ue_mcp_proxy.py  --HTTP :80xx/mcp-->  UnrealEditor (ModelContextProtocol plugin)
```

- `.mcp.json` registers the proxy as server `unreal-mcp`. It is always connected, even when no editor runs.
- The proxy has three own tools: `ue_status`, `ue_start`, `ue_stop`. Everything else comes from the editor in
  tool-search mode (UE 5.9): `list_toolsets` -> `describe_toolsets` (arg `toolset_names`, array; `include_schemas`) /
  `describe_tools` (arg `tool_names`) -> `call_tool`. There is no singular `describe_toolset` in 5.9.
- Each Claude session starts its own editor on a free port in **8010-8099**, so parallel sessions do not collide.
  Instances are recorded in `%LOCALAPPDATA%/ue-mcp/instances/`, logs are in `%LOCALAPPDATA%/ue-mcp/logs/ue_<port>.log`.
- An editor you open yourself serves **:8000** (`bAutoStartServer=True`, `ServerPortNumber=8000` in
  `UE/Config/DefaultEditorPerProjectUserSettings.ini`). The proxy uses it only when the session has no editor of its own.

## Project settings

| What | Where |
|---|---|
| MCP server on/off, port, tool-search | `UE/Config/DefaultEditorPerProjectUserSettings.ini`, section `[/Script/ModelContextProtocol.ModelContextProtocolSettings]` |
| Instructions sent to MCP clients | `UE/Config/DefaultGame.ini`, section `[/Script/ModelContextProtocol.ModelContextProtocolProjectSettings]` |
| Enabled toolsets | `Plugins` list in `UE/TheSagaCutter.uproject` |

Enabled MCP plugins: `ModelContextProtocol`, `ToolsetRegistry`, `MCPClientToolset`, `EditorToolset`, the aggregator `AllToolsets`
(every plugin under `Engine/Plugins/Experimental/Toolsets`), and `UnrealCanvasAnnotations` / `UnrealCanvasGraph`.
If editor startup gets slow or you want a leaner set, remove `AllToolsets` and list only the toolset plugins you need
(they are also listed there individually).

## Using it

1. `ue_status` - does this session have an editor?
2. `ue_start` with `offscreen: true` (no window) unless you want to see the editor. Pass `map` only with an exact `/Game/...` path.
   Wait 10-60 s.
3. `list_toolsets`, `describe_toolsets`, `call_tool` (args `toolset_name`, `tool_name` without the toolset prefix, `arguments`).
   Useful toolsets in 5.9: `EditorToolset.LevelToolset`, `EditorToolset.ActorToolset`, `EditorToolset.AssetToolset`,
   `EditorToolset.EditorAppToolset` (`StartPIE`, `IsPIERunning`, `StopPIE`, camera, selection),
   `editor_toolset.toolsets.python_scripting.PythonScriptingTools` (`read_me` once, then `execute_python`: full `unreal` API,
   the quickest way to build levels), `BlueprintToolsets.BlueprintToolset`, `NavMeshToolset.NavMeshToolset`,
   `FabToolset.FabToolset` (search/install Fab assets: free Viking packs, needs a signed-in Fab session for your library).
4. `ue_stop` when done. Do not leave editors running.

Manual start without Claude:

```bash
python Scripts/mcp/ue_editor.py start --offscreen --map /Game/Vikings/Maps/Map_Fjord
python Scripts/mcp/ue_editor.py list
python Scripts/mcp/ue_editor.py stop --all
```

`UE_ROOT` (default `C:\Programs\UE_5.9`) and `UE_PROJECT` (default: the `.uproject` in `UE/`) override the paths.

## Gotchas

- An unknown `map` path does not fail: the editor opens another level. Check the loaded level, or use `load_level` (it errors if the level is missing).
- `find_actors` returns paths only. Get the class from the actor name or `ObjectTools`.
- MCP servers load at session start. After editing `ue_mcp_proxy.py`, test in a new session.
- "editor returned no response": read the editor log. The proxy strips the client `_meta.progressToken` because the editor answers empty when it is forwarded.
- Builds and asset saves share files between editors: do not build while another editor saves assets.
- After adding C++ classes, build the `TheSagaCutterEditor` target (or use Live Coding) before starting the editor, otherwise new classes are missing.
