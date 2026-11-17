nanobench::Bench().minEpochIterations(1000000).run("add",
[&] {
        result = simple_add(x, y);
        nanobench::doNotOptimizeAway(result);
    });
