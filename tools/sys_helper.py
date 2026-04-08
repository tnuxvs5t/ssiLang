from __future__ import annotations

import sys

from ssinet_py import PipeChannel


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: sys_helper.py <pipe_name>", file=sys.stderr)
        return 2

    pipe_name = sys.argv[1]
    channel = PipeChannel.open_named(pipe_name)
    try:
        while True:
            msg = channel.recv_net()
            cmd = msg.get("cmd")
            if cmd == "echo":
                channel.send_net({"ok": True, "text": msg.get("text")})
                continue
            if cmd == "sum":
                xs = msg.get("xs") or []
                total = 0
                for item in xs:
                    total += item
                channel.send_net({"ok": True, "total": total})
                continue
            if cmd == "stop":
                channel.send_net({"ok": True})
                return 0
            channel.send_net({"ok": False, "error": f"unknown cmd: {cmd}"})
    finally:
        channel.close()


if __name__ == "__main__":
    raise SystemExit(main())
