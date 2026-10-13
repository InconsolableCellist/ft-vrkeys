#!/usr/bin/python3 -I
"""Print every non-touch controller component change from vrserver's web socket."""
import json, os, sys, time
sys.path.insert(0, os.path.expanduser("~/Programs/frametop/input"))
import vrws
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30
ws = vrws.VrSocket(); ws.open(f"ftvrkeys_{os.getpid()}")
sides = vrws.controllers()
print("controllers:", sides, flush=True)
for p in sides: ws.subscribe(p)
start = time.monotonic(); n = 0; last = {}
while time.monotonic() - start < secs:
    for text in ws.messages(timeout=0.5):
        n += 1
        msg = json.loads(text)
        if msg.get("type") != "update_component_states": print("other:", text[:200]); continue
        side = sides.get(msg.get("device"), msg.get("device"))
        for k, v in msg.get("components", {}).items():
            if k.endswith("/touch") or "/capsense/" in k: continue
            if isinstance(v, float): v = round(v, 1)
            if last.get((side, k)) == v: continue
            last[(side, k)] = v
            print(f"{time.monotonic()-start:7.3f} {side} {k} {v}", flush=True)
print(n, "messages")
