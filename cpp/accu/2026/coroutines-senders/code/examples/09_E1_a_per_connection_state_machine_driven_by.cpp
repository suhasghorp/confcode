#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

using ConnId = std::uint64_t;

enum class EventKind { Hello, Auth, Data, Close };

struct Event {
    ConnId conn;
    EventKind kind;
    std::string payload;
};

class Connection {
public:
    enum class State { AwaitingHello, AwaitingAuth, Established, Closed };

    // Called by the reactor. Must never block.
    void on_event(const Event& ev) {
        switch (state_) {
        case State::AwaitingHello:
            if (ev.kind == EventKind::Hello) {
                send("HELLO-ACK");
                state_ = State::AwaitingAuth;
            } else {
                fail("expected HELLO");
            }
            break;
        case State::AwaitingAuth:
            if (ev.kind == EventKind::Auth && ev.payload == "secret") {
                send("AUTH-OK");
                state_ = State::Established;
            } else {
                fail("bad credentials");
            }
            break;
        case State::Established:
            if (ev.kind == EventKind::Data) {
                send("ECHO " + ev.payload);   // stay in Established
            } else if (ev.kind == EventKind::Close) {
                state_ = State::Closed;
            }
            break;
        case State::Closed:
            break;                            // ignore late events
        }
    }

    bool closed() const { return state_ == State::Closed; }

private:
    // Stand-in for a non-blocking write that just queues bytes.
    void send(const std::string& msg) { std::cout << "-> " << msg << '\n'; }
    void fail(const std::string& why) {
        std::cout << "error: " << why << '\n';
        state_ = State::Closed;
    }

    State state_ = State::AwaitingHello;
};

class Reactor {
public:
    void dispatch(const Event& ev) {
        // Map lookup: find (or create) the state machine for this flow.
        auto [it, inserted] = connections_.try_emplace(ev.conn);
        it->second.on_event(ev);
        if (it->second.closed()) connections_.erase(it);
    }

private:
    std::unordered_map<ConnId, Connection> connections_;
};

int main() {
    Reactor reactor;
    // Events from two clients arrive interleaved, as from a real network.
    const Event events[] = {
        {1, EventKind::Hello, ""},
        {2, EventKind::Hello, ""},
        {1, EventKind::Auth,  "secret"},
        {2, EventKind::Auth,  "wrong"},
        {1, EventKind::Data,  "ping"},
        {1, EventKind::Close, ""},
    };
    for (const auto& ev : events) reactor.dispatch(ev);   // the event loop
}

