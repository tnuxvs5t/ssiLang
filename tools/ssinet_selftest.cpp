#include "ssinet.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

using ssilang::net::Error;
using ssilang::net::PipeChannel;
using ssilang::net::SharedMemory;
using ssilang::net::TcpServer;
using ssilang::net::TcpSocket;
using ssilang::net::Value;
using ssilang::net::encode_payload;
using ssilang::net::decode_payload;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw Error(message);
    }
}

Value sample_value() {
    return Value::map({
        {"kind", Value::string("demo")},
        {"id", Value::number(42)},
        {"ok", Value::boolean(true)},
        {"payload", Value::list({
            Value::number(1),
            Value::string("two"),
            Value::null(),
            Value::map({
                {"nested", Value::string("yes")}
            })
        })}
    });
}

} // namespace

int main() {
    try {
        const Value original = sample_value();
        const std::string encoded = encode_payload(original);
        const Value decoded = decode_payload(encoded);
        require(decoded == original, "codec round-trip failed");

        {
            PipeChannel pipe = PipeChannel::anonymous();
            pipe.send_net(original);
            const Value received = pipe.recv_net();
            require(received == original, "pipe round-trip failed");
        }

        {
            SharedMemory shm = SharedMemory::open("ssinet_selftest_slot", 4096);
            shm.store_net(original);
            const auto loaded = shm.load_net();
            require(loaded.has_value(), "shm load returned empty");
            require(*loaded == original, "shm round-trip failed");
            shm.clear();
            require(!shm.load_net().has_value(), "shm clear failed");
        }

        {
            constexpr const char* endpoint = "127.0.0.1:23191";

            TcpServer server = TcpServer::listen(endpoint);
            std::thread worker([&server, &original]() {
                TcpSocket peer = server.accept();
                const Value incoming = peer.recv_net();
                require(incoming == original, "tcp receive mismatch");
                peer.send_net(Value::map({
                    {"status", Value::string("ok")},
                    {"echo", incoming}
                }));
            });

            TcpSocket client = TcpSocket::connect(endpoint, 3);
            client.send_net(original);
            const Value reply = client.recv_net();
            const Value* status = reply.find("status");
            const Value* echo = reply.find("echo");
            require(status != nullptr && status->is_string() && status->as_string() == "ok", "tcp reply status mismatch");
            require(echo != nullptr && *echo == original, "tcp reply echo mismatch");

            worker.join();
        }

        std::cout << "ssinet selftest passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ssinet selftest failed: " << e.what() << "\n";
        return 1;
    }
}
