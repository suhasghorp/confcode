# C++: separate translation units, then whole-program LTO
g++-13 -std=c++20 -O3 -march=native cpp/battle1_bench.cc cpp/battle1_factory.cc \
    -lbenchmark -lpthread -o battle1_o3
g++-13 -std=c++20 -O3 -march=native -flto cpp/battle1_bench.cc cpp/battle1_factory.cc \
    -lbenchmark -lpthread -o battle1_flto
taskset -c 2 ./battle1_o3 --benchmark_repetitions=5 --benchmark_report_aggregates_only=true

# Java: compile against the JMH jars, running the JMH annotation processor
M2=$HOME/.m2/repository
CP=$M2/org/openjdk/jmh/jmh-core/1.37/jmh-core-1.37.jar:$M2/net/sf/jopt-simple/jopt-simple/5.0.4/jopt-simple-5.0.4.jar:$M2/org/apache/commons/commons-math3/3.6.1/commons-math3-3.6.1.jar
AP=$M2/org/openjdk/jmh/jmh-generator-annprocess/1.37/jmh-generator-annprocess-1.37.jar
javac --release 21 -cp "$CP" -processorpath "$AP:$CP" -d out java/bench/*.java

# Allocation per operation, then hardware counters per operation
taskset -c 2,3 java -cp "out:$CP" org.openjdk.jmh.Main Battle1 -prof gc
taskset -c 2,3 java -cp "out:$CP" org.openjdk.jmh.Main Battle2Bench \
    -prof 'perfnorm:events=LLC-load-misses,dTLB-load-misses,instructions,cycles'

# Tail latency, once per collector
java -XX:+UseG1GC -Xms4g -Xmx4g -XX:+AlwaysPreTouch -Xlog:gc -cp out bench.Battle3Tail G1-4g
java -XX:+UseZGC -XX:+ZGenerational -Xms4g -Xmx4g -XX:+AlwaysPreTouch -cp out bench.Battle3Tail ZGC-4g
