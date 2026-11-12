#!/usr/bin/env python3
"""ue_mcp_proxy.py - stdio MCP server that forwards to the Unreal MCP server of "this session's" editor.

Why: the Unreal MCP plugin is an HTTP server inside the editor. A plain http entry in .mcp.json fails when
the session starts before the editor (ECONNREFUSED, no reconnect), and parallel sessions cannot share
one fixed port. This proxy is always connected. It finds the editor with ue_editor.pick() on every call
(same Claude session > same project > user's editor on :8000), re-initializes after an editor restart,
and sends tools/list_changed when an editor comes or goes.

Own tools: ue_status, ue_start, ue_stop. All other tools come from the editor (tool-search mode:
list_toolsets, describe_toolset, call_tool). Without an editor, the last known editor tool list is shown;
a call then returns "call ue_start first".
.mcp.json: {"mcpServers": {"unreal-mcp": {"command": "python", "args": ["Scripts/mcp/ue_mcp_proxy.py"]}}}
"""
import json, os, sys, threading, time, urllib.request, urllib.error

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue_editor  # noqa: E402

SESSION = os.environ.get("CLAUDE_CODE_SESSION_ID", "")
PROJECT = ue_editor.project_file()
CACHE = os.path.join(ue_editor.REG, "..", "tools_cache.json")
OUT_LOCK = threading.Lock()
LOGF = os.path.join(ue_editor.REG, "..", "logs", "proxy_%d.log" % os.getpid())

OWN_TOOLS = [
    {"name": "ue_status", "description": "Unreal MCP proxy: which editor instance this session uses, and all live instances.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "ue_start", "description": "Start an Unreal Editor for this session with the MCP server on a free port, wait until it "
     "answers (1-3 min), then the editor tools work. Reuses this session's editor if it already runs.",
     "inputSchema": {"type": "object", "properties": {
         "map": {"type": "string", "description": "Optional map to open, for example /Game/Vikings/Maps/Map_Fjord"},
         "offscreen": {"type": "boolean", "description": "true = no editor window (-RenderOffscreen). Default false."},
         "wait": {"type": "integer", "description": "Max seconds to wait for MCP (default 300)"}}}},
    {"name": "ue_stop", "description": "Stop the Unreal Editor that this session started (or one port).",
     "inputSchema": {"type": "object", "properties": {"port": {"type": "integer"}}}},
]
STATIC_UE_TOOLS = [  # used only before the first editor connection ever (no cache yet)
    {"name": "list_toolsets", "description": "List the Unreal Editor toolsets. Needs a running editor (ue_start).",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "describe_toolset", "description": "Describe the tools and input schemas of one Unreal toolset.",
     "inputSchema": {"type": "object", "properties": {"toolset_name": {"type": "string"}}, "required": ["toolset_name"]}},
    {"name": "call_tool", "description": "Call a tool of an Unreal toolset.",
     "inputSchema": {"type": "object", "properties": {"toolset_name": {"type": "string"}, "tool_name": {"type": "string"},
                                                      "arguments": {"type": "object"}}, "required": ["tool_name"]}},
]


def log(msg):
    try:
        os.makedirs(os.path.dirname(LOGF), exist_ok=True)
        with open(LOGF, "a", encoding="utf-8") as f:
            f.write("%s %s\n" % (time.strftime("%H:%M:%S"), msg))
    except OSError:
        pass


def send(obj):
    data = json.dumps(obj, ensure_ascii=False)
    with OUT_LOCK:
        sys.stdout.write(data + "\n")
        sys.stdout.flush()


class Upstream:
    """Streamable-HTTP MCP client for one editor port. Re-initializes when the session is lost."""

    def __init__(self):
        self.lock = threading.Lock()
        self.port = None
        self.sid = None
        self.proto = "2025-06-18"
        self.ids = 0

    def _post(self, port, msg, timeout):
        req = urllib.request.Request("http://127.0.0.1:%d%s" % (port, ue_editor.URL_PATH), data=json.dumps(msg).encode(),
                                     method="POST", headers=dict({"Content-Type": "application/json",
                                                                  "Accept": "application/json, text/event-stream"},
                                                                 **({"Mcp-Session-Id": self.sid} if self.sid else {})))
        with urllib.request.urlopen(req, timeout=timeout) as r:
            sid = r.headers.get("Mcp-Session-Id")
            ctype = r.headers.get("Content-Type", "")
            body = r.read().decode("utf-8", "replace")
        if "id" not in msg:
            return sid, None
        if "text/event-stream" in ctype:
            res = None
            for line in body.splitlines():
                if line.startswith("data:"):
                    try:
                        m = json.loads(line[5:].strip())
                    except ValueError:
                        continue
                    if m.get("id") == msg["id"]:
                        res = m
            return sid, res
        return sid, (json.loads(body) if body.strip() else None)

    def _init(self, port):
        self.sid = None
        sid, res = self._post(port, {"jsonrpc": "2.0", "id": "init", "method": "initialize", "params": {
            "protocolVersion": self.proto, "capabilities": {}, "clientInfo": {"name": "ue_mcp_proxy", "version": "1"}}}, 15)
        self.sid = sid
        self._post(port, {"jsonrpc": "2.0", "method": "notifications/initialized"}, 15)
        self.port = port
        log("connected :%d sid=%s" % (port, sid))

    def call(self, method, params, timeout=1800):
        """Forward one request to the picked editor. Returns the JSON-RPC response (dict) or raises."""
        inst = ue_editor.pick(SESSION, PROJECT)
        if not inst:
            raise ConnectionError("no editor")
        port = inst["port"]
        with self.lock:
            if self.port != port or not self.sid:
                self._init(port)
            self.ids += 1
            rid = "p%d" % self.ids
        params = {k: v for k, v in (params or {}).items() if k != "_meta"}  # progressToken makes the editor reply empty
        msg = {"jsonrpc": "2.0", "id": rid, "method": method, "params": params or {}}
        try:
            _, res = self._post(port, msg, timeout)
        except urllib.error.HTTPError as e:
            if e.code in (400, 404):  # session lost (editor restarted): re-init once
                with self.lock:
                    self._init(port)
                _, res = self._post(port, msg, timeout)
            else:
                raise
        except (urllib.error.URLError, OSError):
            with self.lock:
                self.port = self.sid = None
            raise
        return res


UP = Upstream()
STATE = {"connected": None, "tools": None}


def editor_tools():
    """Tool list of the editor: live > cache > static."""
    try:
        res = UP.call("tools/list", {}, timeout=30)
        tools = (res or {}).get("result", {}).get("tools")
        if tools is not None:
            try:
                with open(CACHE, "w", encoding="utf-8") as f:
                    json.dump(tools, f)
            except OSError:
                pass
            return tools
    except Exception as e:  # noqa: BLE001
        log("tools/list upstream failed: %r" % e)
    try:
        with open(CACHE, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return STATIC_UE_TOOLS


def text(t, err=False):
    return {"content": [{"type": "text", "text": t}], "isError": err}


def own_call(name, args):
    if name == "ue_status":
        inst = ue_editor.pick(SESSION, PROJECT)
        cur = ("using port %d (%s)" % (inst["port"], inst["why"])) if inst else "no editor for this session"
        return text("%s\nsession=%s project=%s\ninstances:\n%s" % (cur, SESSION[:8] or "-", PROJECT, ue_editor.fmt_list()))
    if name == "ue_start":
        ok, msg = ue_editor.start(args.get("map"), bool(args.get("offscreen")), None, int(args.get("wait") or 300),
                                  (), PROJECT, SESSION)
        watch_once()
        return text(msg, not ok)
    if name == "ue_stop":
        msg = ue_editor.stop(args.get("port"), False, SESSION)
        watch_once()
        return text(msg)
    return None


def handle(msg):
    mid, method, params = msg.get("id"), msg.get("method"), msg.get("params") or {}
    if mid is None:
        return  # notification from the client: nothing to forward
    try:
        if method == "initialize":
            UP.proto = params.get("protocolVersion") or UP.proto
            res = {"protocolVersion": UP.proto, "capabilities": {"tools": {"listChanged": True}},
                   "serverInfo": {"name": "unreal-mcp-proxy", "version": "1.0"},
                   "instructions": "Unreal Editor tools. If a call says no editor runs, call ue_start, then retry."}
        elif method == "ping":
            res = {}
        elif method == "tools/list":
            res = {"tools": OWN_TOOLS + editor_tools()}
        elif method == "tools/call":
            name, args = params.get("name"), params.get("arguments") or {}
            res = own_call(name, args)
            if res is None:
                try:
                    up = UP.call("tools/call", params)
                    if up is None:
                        res = text("editor returned no response", True)
                    elif "error" in up:
                        send({"jsonrpc": "2.0", "id": mid, "error": up["error"]}); return
                    else:
                        res = up.get("result")
                except ConnectionError:
                    res = text("No Unreal Editor with MCP for this session. Call ue_start (or start the editor "
                               "yourself: it serves :8000), then retry.", True)
                except Exception as e:  # noqa: BLE001
                    res = text("Unreal MCP call failed: %r. The editor may be busy or closed; check ue_status." % e, True)
        elif method in ("resources/list", "prompts/list"):
            res = {method.split("/")[0]: []}
        else:
            send({"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": "method not found: %s" % method}})
            return
        send({"jsonrpc": "2.0", "id": mid, "result": res})
    except Exception as e:  # noqa: BLE001
        log("handle %s failed: %r" % (method, e))
        send({"jsonrpc": "2.0", "id": mid, "error": {"code": -32603, "message": repr(e)}})


def watch_once():
    """Send tools/list_changed when the editor connection changes (comes, goes, other port)."""
    inst = ue_editor.pick(SESSION, PROJECT)
    key = inst["port"] if inst and ue_editor.mcp_ping(inst["port"], 2) else None
    if key != STATE["connected"]:
        STATE["connected"] = key
        log("editor now %s" % key)
        send({"jsonrpc": "2.0", "method": "notifications/tools/list_changed"})


def watcher():
    while True:
        time.sleep(10)
        try:
            watch_once()
        except Exception as e:  # noqa: BLE001
            log("watch failed: %r" % e)


def main():
    for s in (sys.stdin, sys.stdout):  # newline-delimited JSON: no \r\n translation, UTF-8 on Windows
        s.reconfigure(encoding="utf-8", newline="\n")
    log("proxy start session=%s project=%s" % (SESSION, PROJECT))
    threading.Thread(target=watcher, daemon=True).start()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        for m in (msg if isinstance(msg, list) else [msg]):
            threading.Thread(target=handle, args=(m,), daemon=True).start()


if __name__ == "__main__":
    main()
