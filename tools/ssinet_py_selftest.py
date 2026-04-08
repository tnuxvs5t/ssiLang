from __future__ import annotations

import threading

from ssinet_py import PipeChannel, SharedMemory, TcpServer, TcpSocket, decode_payload, encode_payload


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sample_value():
    return {
        "kind": "demo",
        "id": 42,
        "ok": True,
        "payload": [
            1,
            "two",
            None,
            {"nested": "yes"},
        ],
    }


def main() -> None:
    original = sample_value()

    encoded = encode_payload(original)
    decoded = decode_payload(encoded)
    require(decoded == original, "codec round-trip failed")

    pipe = PipeChannel.anonymous()
    try:
        pipe.send_net(original)
        received = pipe.recv_net()
        require(received == original, "pipe round-trip failed")
    finally:
        pipe.close()

    shm = SharedMemory.open("ssinet_py_selftest_slot", 4096)
    try:
        shm.store_net(original)
        loaded = shm.load_net()
        require(loaded == original, "shm round-trip failed")
        shm.clear()
        require(shm.load_net() is None, "shm clear failed")
    finally:
        shm.close()

    endpoint = "127.0.0.1:23192"
    server = TcpServer.listen(endpoint)

    def worker() -> None:
        peer = server.accept()
        try:
            incoming = peer.recv_net()
            require(incoming == original, "tcp receive mismatch")
            peer.send_net({"status": "ok", "echo": incoming})
        finally:
            peer.close()

    thread = threading.Thread(target=worker)
    thread.start()

    client = TcpSocket.connect(endpoint, 3)
    try:
        client.send_net(original)
        reply = client.recv_net()
        require(reply["status"] == "ok", "tcp reply status mismatch")
        require(reply["echo"] == original, "tcp reply echo mismatch")
    finally:
        client.close()
        server.close()

    thread.join()
    print("ssinet_py selftest passed")


if __name__ == "__main__":
    main()
