#include <iostream>
#include <utility>

// Innermost layer: a sender that produces the value 2.
struct before_sender {
    template <class Rcv>
    struct op {
        Rcv rcv;
        void start() {
            std::cout << "before: producing 2\n";
            rcv.set_value(2);          // completion starts at the innermost receiver
        }
    };
    template <class Rcv>
    op<Rcv> connect(Rcv r) const {
        std::cout << "connect(before_snd, then_f_rcv)\n";
        return op<Rcv>{std::move(r)};
    }
};

// Middle layer: then(f) wraps an inner sender.
template <class Inner, class F>
struct then_sender {
    Inner inner;
    F f;

    template <class Next>
    struct rcv_t {                     // the receiver this adaptor generates
        Next next;
        F f;
        void set_value(int v) {
            std::cout << "then_f_rcv got " << v << '\n';
            next.set_value(f(v));      // apply f, pass the result outward
        }
    };

    template <class Next>
    auto connect(Next r) const {
        std::cout << "connect(then_f_snd, after_rcv)\n";
        return inner.connect(rcv_t<Next>{std::move(r), f});  // recurse inward
    }
};

// Outer layer: after wraps an inner sender.
template <class Inner>
struct after_sender {
    Inner inner;

    template <class Next>
    struct rcv_t {
        Next next;
        void set_value(int v) {
            std::cout << "after_rcv got " << v << '\n';
            next.set_value(v);
        }
    };

    template <class Next>
    auto connect(Next r) const {
        std::cout << "connect(after_snd, final_rcv)\n";
        return inner.connect(rcv_t<Next>{std::move(r)});     // recurse inward
    }
};

// Whoever consumes the whole pipeline.
struct final_receiver {
    void set_value(int v) { std::cout << "final result: " << v << '\n'; }
};

int main() {
    auto f = [](int x) { return x * 2; };

    // Build the sender onion: after wraps then(f), which wraps before.
    before_sender before_snd{};
    then_sender<before_sender, decltype(f)> then_f_snd{before_snd, f};
    after_sender<decltype(then_f_snd)> after_snd{then_f_snd};

    // connect: outside-in. Produces one nested operation state on the stack.
    auto op = after_snd.connect(final_receiver{});

    // start: completion flows inside-out through the receiver onion.
    op.start();
}

