#!/usr/bin/env bash
# Build the C++ (Google Benchmark, GCC 13) and Java 21 (JMH 1.37) benchmarks.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p out/cpp out/java

CXX=${CXX:-g++-13}
FLAGS="-std=c++20 -O3 -march=native -DNDEBUG"
GB="-I/usr/local/include -L/usr/local/lib -lbenchmark -lpthread"

# Battle 1 twice: separate translation units, then whole-program LTO.
$CXX $FLAGS cpp/battle1_bench.cc cpp/battle1_factory.cc $GB -o out/cpp/battle1_o3
$CXX $FLAGS -flto cpp/battle1_bench.cc cpp/battle1_factory.cc $GB -o out/cpp/battle1_flto
$CXX $FLAGS cpp/battle2_bench.cc $GB -o out/cpp/battle2
$CXX $FLAGS cpp/battle3_tail.cc -o out/cpp/battle3_tail

JDK=${JDK:-$HOME/.sdkman/candidates/java/21.0.6-librca}
M2=$HOME/.m2/repository
CP=$M2/org/openjdk/jmh/jmh-core/1.37/jmh-core-1.37.jar:$M2/net/sf/jopt-simple/jopt-simple/5.0.4/jopt-simple-5.0.4.jar:$M2/org/apache/commons/commons-math3/3.6.1/commons-math3-3.6.1.jar
AP=$M2/org/openjdk/jmh/jmh-generator-annprocess/1.37/jmh-generator-annprocess-1.37.jar
"$JDK/bin/javac" --release 21 -cp "$CP" -processorpath "$AP:$CP" -d out/java java/bench/*.java
echo "$CP" > out/java.classpath
echo built
