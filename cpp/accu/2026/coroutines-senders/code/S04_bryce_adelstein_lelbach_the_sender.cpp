before | then(f) | after;

sender auto before_snd = …;
sender auto then_f_snd = then_sender(before_snd, f);
sender auto after_snd  = after_sender(then_f_snd);
…
    return connect(after_snd, …);
       return connect(then_f_snd, after_rcv);
          return connect(before_snd, then_f_rcv);
…

…
    set_value(before_rcv, …);
       set_value(then_f_rcv, before_val);
          set_value(after_rcv, f(before_val));
…
