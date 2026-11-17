        std::jthread t1([](){auto& x = get_once(10);}),
        t2([](){auto& x = get_once(11);});
}
