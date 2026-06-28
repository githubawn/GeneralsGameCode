#!/usr/bin/env python3
# TheSuperHackers @feature githubawn 27/06/2026 WebSocket UDP relay for the WebAssembly
# build's LAN multiplayer. Browsers cannot open raw UDP sockets, so the web build's UDP
# class (Core/.../GameNetwork/udp.cpp, __EMSCRIPTEN__ path) tunnels every datagram over a
# single WebSocket to this relay. The relay assigns each connected browser a virtual LAN
# IP (10.0.0.2, 10.0.0.3, ...) and routes datagrams between them: unicast by destination
# virtual IP, and 255.255.255.255 (INADDR_BROADCAST, used by LANAPI host discovery)
# fanned out to every other client. This is a dev/loopback relay — no auth, trusts framing.
#
# Wire framing (WebSocket BINARY frame = one UDP datagram), big-endian header:
#   [0:4]  srcIP    (filled/overwritten by relay with the sender's assigned IP)
#   [4:6]  srcPort
#   [6:10] dstIP    (10.0.0.x for unicast, 0xFFFFFFFF for broadcast)
#   [10:12] dstPort
#   [12:]  payload
# Control (WebSocket TEXT frame), relay -> client only:
#   "IP a.b.c.d"   sent once on connect to tell the client its assigned virtual IP.

import socket
import struct
import threading
import hashlib
import base64

HOST = "0.0.0.0"
PORT = 8090
WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
BROADCAST_IP = 0xFFFFFFFF
HEADER = struct.Struct(">IHIH")  # srcIP, srcPort, dstIP, dstPort

_lock = threading.Lock()
_clients = {}          # assigned_ip(int) -> Client
_next_host = 2         # 10.0.0.2 upward
_dgram_count = 0       # routed-datagram counter (for throttled logging)


def ip_to_int(a, b, c, d):
    return (a << 24) | (b << 16) | (c << 8) | d


def int_to_ip(v):
    return "%d.%d.%d.%d" % ((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255)


class Client:
    def __init__(self, conn, ip):
        self.conn = conn
        self.ip = ip            # fallback auto-assigned IP (10.0.0.x)
        self.declared_ip = ip   # identity the client actually uses (e.g. selected 127.0.0.N)
        self.send_lock = threading.Lock()

    def send_frame(self, payload, opcode):
        # Server->client frames are never masked.
        n = len(payload)
        if n < 126:
            header = struct.pack(">BB", 0x80 | opcode, n)
        elif n < 65536:
            header = struct.pack(">BBH", 0x80 | opcode, 126, n)
        else:
            header = struct.pack(">BBQ", 0x80 | opcode, 127, n)
        with self.send_lock:
            try:
                self.conn.sendall(header + payload)
            except OSError:
                pass

    def send_text(self, s):
        self.send_frame(s.encode("utf-8"), 0x1)

    def send_binary(self, b):
        self.send_frame(b, 0x2)


def ws_handshake(conn):
    """Read the HTTP upgrade request and reply with the Sec-WebSocket-Accept."""
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(1024)
        if not chunk:
            return False
        data += chunk
        if len(data) > 65536:
            return False
    key = None
    for line in data.split(b"\r\n"):
        if line.lower().startswith(b"sec-websocket-key:"):
            key = line.split(b":", 1)[1].strip().decode()
            break
    if not key:
        return False
    accept = base64.b64encode(hashlib.sha1((key + WS_MAGIC).encode()).digest()).decode()
    resp = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
    )
    conn.sendall(resp.encode())
    return True


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def read_frame(conn):
    """Return (opcode, payload) for one client frame, or (None, None) on close/EOF."""
    h = recv_exact(conn, 2)
    if not h:
        return None, None
    b0, b1 = h[0], h[1]
    opcode = b0 & 0x0F
    masked = (b1 & 0x80) != 0
    ln = b1 & 0x7F
    if ln == 126:
        ext = recv_exact(conn, 2)
        if not ext:
            return None, None
        ln = struct.unpack(">H", ext)[0]
    elif ln == 127:
        ext = recv_exact(conn, 8)
        if not ext:
            return None, None
        ln = struct.unpack(">Q", ext)[0]
    mask = b"\x00\x00\x00\x00"
    if masked:
        mask = recv_exact(conn, 4)
        if not mask:
            return None, None
    payload = recv_exact(conn, ln) if ln else b""
    if payload is None:
        return None, None
    if masked and ln:
        payload = bytes(payload[i] ^ mask[i & 3] for i in range(ln))
    return opcode, payload


def route_datagram(sender, payload):
    if len(payload) < HEADER.size:
        return
    src_ip, _src_port, dst_ip, _dst_port = HEADER.unpack(payload[:HEADER.size])
    # The client declares its own identity (the IP it selected in Options) in the src
    # field. Learn it so peers can reach this client by that IP, and forward the frame
    # unchanged (keep the declared src so the receiver replies to the right address).
    if src_ip != 0:
        sender.declared_ip = src_ip
    frame = payload
    with _lock:
        targets = list(_clients.values())
    global _dgram_count
    _dgram_count += 1
    # Throttle: in-game lockstep produces a steady stream of datagrams, so only log the first
    # few (handshake/discovery) and then a periodic heartbeat to confirm traffic still flows.
    if _dgram_count <= 12 or _dgram_count % 500 == 0:
        kind = "BCAST" if dst_ip == BROADCAST_IP else "unicast->" + int_to_ip(dst_ip)
        delivered = (len(targets) - 1) if dst_ip == BROADCAST_IP else 1
        print("[relay] dgram #%d %s src=%s len=%d -> %d peer(s)" %
              (_dgram_count, kind, int_to_ip(src_ip), len(payload) - HEADER.size, delivered))
    if dst_ip == BROADCAST_IP:
        for c in targets:
            if c is not sender:
                c.send_binary(frame)
    else:
        for c in targets:
            if c.declared_ip == dst_ip and c is not sender:
                c.send_binary(frame)
                break


def handle(conn, peer):
    global _next_host
    conn.settimeout(None)
    client = None
    try:
        if not ws_handshake(conn):
            conn.close()
            return
        with _lock:
            host = _next_host
            _next_host += 1
            ip = ip_to_int(10, 0, 0, host)
            client = Client(conn, ip)
            _clients[ip] = client
            count = len(_clients)
        client.send_text("IP " + int_to_ip(ip))
        print("[relay] %s connected as %s (%d clients)" % (peer, int_to_ip(ip), count))
        while True:
            opcode, payload = read_frame(conn)
            if opcode is None or opcode == 0x8:  # EOF or close
                break
            if opcode == 0x2:                     # binary = datagram
                route_datagram(client, payload)
            elif opcode == 0x9:                   # ping -> pong
                client.send_frame(payload, 0xA)
    except OSError:
        pass
    finally:
        if client is not None:
            with _lock:
                _clients.pop(client.ip, None)
        try:
            conn.close()
        except OSError:
            pass
        print("[relay] %s disconnected" % (peer,))


def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))
    srv.listen(16)
    print("=" * 60)
    print("C&C Generals Zero Hour — WebSocket LAN relay")
    print("Listening: ws://localhost:%d" % PORT)
    print("=" * 60)
    try:
        while True:
            conn, peer = srv.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            threading.Thread(target=handle, args=(conn, peer), daemon=True).start()
    except KeyboardInterrupt:
        print("\nShutting down relay.")
    finally:
        srv.close()


if __name__ == "__main__":
    main()
