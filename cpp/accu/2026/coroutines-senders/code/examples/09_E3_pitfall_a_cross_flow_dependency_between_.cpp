#include <iostream>
#include <unordered_map>
#include <vector>

using ConnId = int;

struct Session {
    enum class State { Established, WaitingForFeed, Trading };
    State state = State::Established;
};

class Reactor {
public:
    // A client may only start trading once the market-data feed is live.
    void on_start_trading(ConnId id) {
        auto& s = sessions_[id];
        if (feed_live_) {
            s.state = Session::State::Trading;
            std::cout << id << ": trading\n";
        } else {
            s.state = Session::State::WaitingForFeed;   // state caused by ANOTHER flow
            waiting_.push_back(id);
            std::cout << id << ": waiting for feed\n";
        }
    }

    // An event on a different flow must now wake up other state machines.
    void on_feed_live() {
        feed_live_ = true;
        for (ConnId id : waiting_) {
            auto it = sessions_.find(id);
            if (it == sessions_.end()) continue;        // client left meanwhile!
            it->second.state = Session::State::Trading;
            std::cout << id << ": trading (woken by feed)\n";
        }
        waiting_.clear();
    }

    // Note: leaves a stale id in waiting_. Easy to forget, easy to get wrong.
    void on_disconnect(ConnId id) { sessions_.erase(id); }

    // Still unanswered: feed going down again? timeouts while waiting?
    // cancelling a wait? Each adds more states and more shared bookkeeping.

private:
    std::unordered_map<ConnId, Session> sessions_;
    std::vector<ConnId> waiting_;
    bool feed_live_ = false;
};

int main() {
    Reactor r;
    r.on_start_trading(1);
    r.on_start_trading(2);
    r.on_disconnect(2);
    r.on_feed_live();
    r.on_start_trading(3);
}

