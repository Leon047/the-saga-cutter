#!/usr/bin/env python3
"""ue_editor.py - start/stop Unreal Editor instances with the Unreal MCP server on a free port.

Each instance is recorded in a registry (%LOCALAPPDATA%/ue-mcp/instances/<port>.json) with its port,
pid, project and owner (env CLAUDE_CODE_SESSION_ID). ue_mcp_proxy.py reads the registry and connects
the Claude session to "its" instance. Output is short on purpose.

  python Scripts/mcp/ue_editor.py start [--map /Game/Vikings/Maps/Map_Fjord] [--offscreen] [--port N] [--wait 300] [-- extra args]
  python Scripts/mcp/ue_editor.py stop  [--port N | --all]      (default: instances of this session)
  python Scripts/mcp/ue_editor.py list
Env: UE_ROOT (default C:/Programs/UE_5.9), UE_PROJECT (default: *.uproject in UE/ under the git root or cwd).
"""
import argparse, glob, json, os, socket, subprocess, sys, time, urllib.request, urllib.error

UE_ROOT = os.environ.get("UE_ROOT", r"C:\Programs\UE_5.9")
PROJECT_SUBDIRS = ("UE", "")  # the UE project lives in <repo>/UE; "" also accepts a uproject at the repo root
REG = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"), "ue-mcp", "instances")
URL_PATH = "/mcp"
DEFAULT_PORT = 8000          # the port a user-started editor uses (plugin default)
PORT_RANGE = range(8010, 8100)


def project_file(explicit=None):
    p = explicit or os.environ.get("UE_PROJECT")
    if p:
        return os.path.abspath(p)
    try:
        root = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True).stdout.strip()
    except OSError:
        root = ""
    here = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # repo root of this script
    for d in (root, os.getcwd(), here):
        if d:
            for sub in PROJECT_SUBDIRS:
                fs = glob.glob(os.path.join(d, sub, "*.uproject"))
                if fs:
                    return os.path.abspath(fs[0])
    return None


def norm(p):
    return os.path.normcase(os.path.abspath(p)) if p else ""


def pid_alive(pid):
    if not pid:
        return False
    if os.name == "nt":
        r = subprocess.run(["tasklist", "/FI", "PID eq %d" % pid, "/NH"], capture_output=True, text=True)
        return str(pid) in r.stdout
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def port_free(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(("127.0.0.1", port)) != 0


def mcp_ping(port, timeout=3):
    """True if an MCP server answers initialize on this port."""
    body = json.dumps({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {
        "protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "ue_editor-ping", "version": "1"}}}).encode()
    req = urllib.request.Request("http://127.0.0.1:%d%s" % (port, URL_PATH), data=body, method="POST", headers={
        "Content-Type": "application/json", "Accept": "application/json, text/event-stream"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status == 200
    except (urllib.error.URLError, OSError, ValueError):
        return False


def instances(clean=True):
    """Live registry entries (dead ones removed)."""
    out = []
    for f in glob.glob(os.path.join(REG, "*.json")):
        try:
            with open(f, encoding="utf-8") as fh:
                d = json.load(fh)
        except (OSError, ValueError):
            continue
        if pid_alive(d.get("pid")):
            out.append(d)
        elif clean:
            try:
                os.remove(f)
            except OSError:
                pass
    return sorted(out, key=lambda d: -d.get("started", 0))


def pick(session=None, project=None):
    """Instance for a client: same session > same project > user's editor on DEFAULT_PORT. Returns dict or None."""
    pin = os.environ.get("UE_MCP_PORT")
    if pin:
        return {"port": int(pin), "pid": 0, "why": "UE_MCP_PORT"}
    live = instances()
    for d in live:
        if session and d.get("owner") == session:
            return dict(d, why="session")
    for d in live:
        if project and norm(d.get("project")) == norm(project):
            return dict(d, why="project")
    if mcp_ping(DEFAULT_PORT, timeout=1):
        return {"port": DEFAULT_PORT, "pid": 0, "why": "user editor :%d" % DEFAULT_PORT}
    return None


def start(map_=None, offscreen=False, port=None, wait=300, extra=(), project=None, owner=None):
    """Start an editor; return (ok, message). Blocks until the MCP server answers or `wait` s pass."""
    proj = project_file(project)
    if not proj:
        return False, "no .uproject found (set UE_PROJECT)"
    owner = owner if owner is not None else os.environ.get("CLAUDE_CODE_SESSION_ID", "")
    for d in instances():
        if owner and d.get("owner") == owner and norm(d.get("project")) == norm(proj):
            if mcp_ping(d["port"]):
                return True, "already running: port %d pid %d" % (d["port"], d["pid"])
    if port is None:
        port = next((p for p in PORT_RANGE if port_free(p)), None)
        if port is None:
            return False, "no free port in %d-%d" % (PORT_RANGE[0], PORT_RANGE[-1])
    exe = os.path.join(UE_ROOT, "Engine", "Binaries", "Win64", "UnrealEditor.exe")
    if not os.path.isfile(exe):
        return False, "editor not found: " + exe
    logdir = os.path.join(REG, "..", "logs")
    os.makedirs(logdir, exist_ok=True)
    log = os.path.abspath(os.path.join(logdir, "ue_%d.log" % port))
    cmd = [exe, proj] + ([map_] if map_ else []) + [
        "-ModelContextProtocolStartServer", "-ModelContextProtocolPort=%d" % port,
        "-nosplash", "-unattended", "-NoSound", "-abslog=" + log] + (["-RenderOffscreen"] if offscreen else []) + list(extra)
    flags = (subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP) if os.name == "nt" else 0
    p = subprocess.Popen(cmd, creationflags=flags, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, close_fds=True)
    os.makedirs(REG, exist_ok=True)
    rec = {"port": port, "pid": p.pid, "project": proj, "owner": owner, "started": time.time(),
           "map": map_ or "", "offscreen": offscreen, "log": log, "cwd": os.getcwd()}
    with open(os.path.join(REG, "%d.json" % port), "w", encoding="utf-8") as f:
        json.dump(rec, f, indent=1)
    t0 = time.time()
    while time.time() - t0 < wait:
        if mcp_ping(port):
            return True, "ready: port %d pid %d (%ds) log %s" % (port, p.pid, time.time() - t0, log)
        if p.poll() is not None:
            return False, "editor exited rc=%s before MCP was ready. log %s" % (p.returncode, log)
        time.sleep(2)
    return False, "timeout %ds: editor pid %d runs, MCP not ready. log %s" % (wait, p.pid, log)


def stop(port=None, all_=False, owner=None):
    owner = owner if owner is not None else os.environ.get("CLAUDE_CODE_SESSION_ID", "")
    done = []
    for d in instances():
        if all_ or (port and d["port"] == port) or (not port and owner and d.get("owner") == owner):
            if os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(d["pid"]), "/T"], capture_output=True)
                for _ in range(20):
                    if not pid_alive(d["pid"]):
                        break
                    time.sleep(1)
                if pid_alive(d["pid"]):
                    subprocess.run(["taskkill", "/PID", str(d["pid"]), "/T", "/F"], capture_output=True)
            else:
                os.kill(d["pid"], 15)
            try:
                os.remove(os.path.join(REG, "%d.json" % d["port"]))
            except OSError:
                pass
            done.append("%d" % d["port"])
    return "stopped: " + (",".join(done) or "none")


def fmt_list():
    rows = ["%d pid=%d owner=%s map=%s %s" % (d["port"], d["pid"], (d.get("owner") or "-")[:8], d.get("map") or "-",
                                              os.path.basename(d.get("project", ""))) for d in instances()]
    if mcp_ping(DEFAULT_PORT, timeout=1):
        rows.append("%d user editor (not in registry)" % DEFAULT_PORT)
    return "\n".join(rows) or "none"


def main():
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        i = argv.index("--")
        argv, extra = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd")
    p = sp.add_parser("start"); p.add_argument("--map"); p.add_argument("--offscreen", action="store_true")
    p.add_argument("--port", type=int); p.add_argument("--wait", type=int, default=300); p.add_argument("--project")
    p = sp.add_parser("stop"); p.add_argument("--port", type=int); p.add_argument("--all", action="store_true")
    sp.add_parser("list")
    a = ap.parse_args(argv)
    if a.cmd == "start":
        ok, msg = start(a.map, a.offscreen, a.port, a.wait, extra, a.project)
        print(msg); sys.exit(0 if ok else 1)
    elif a.cmd == "stop":
        print(stop(a.port, a.all))
    elif a.cmd == "list":
        print(fmt_list())
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
