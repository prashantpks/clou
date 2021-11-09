#pragma once

#include <cstdio>
#include <string>
#include <sstream>
#include <unordered_map>
#include <arpa/inet.h>

namespace mon {

class Message;

class Client {
public:
    Client(): f(nullptr) {}
    Client(const char *path): f(nullptr) { connect(path); }
    Client(Client&& other): f(other.f) {
        other.f = nullptr;
    }
    
    Client& operator=(Client&& other) {
        f = other.f; other.f = nullptr;
        return *this;
    }
    
    ~Client() { if (good()) { disconnect(); } }
    
    template <typename Msg>
    void send(const Msg& msg);
    bool recv(Message& msg);
    bool send_then_recv(const Message& out, Message& in);
    
    bool good() const { return f != nullptr; }
    operator bool() const { return good(); }
    
    void send_connect();
    void send_step(const std::string& step, const std::string& func);
    void send_properties(const std::string& func, const std::unordered_map<std::string, std::string>& props);
    
    template <typename T>
    void send_property(const std::string& func, const std::string& key, const T& value) {
        std::stringstream ss;
        ss << value;
        send_properties(func, {{key, ss.str()}});
    }
    
private:
    FILE *f;
    
    void connect(const char *path);
    void disconnect();
    
    template <typename T>
    void write(const T *buf, std::size_t count);
};

template <typename Msg>
void Client::send(const Msg& msg) {
    if (!good()) { return; }
    std::string buf;
    msg.SerializeToString(&buf);
    const uint32_t buflen = htonl(buf.size());
    write(&buflen, 1);
    write(buf.data(), buf.size());
}

}
