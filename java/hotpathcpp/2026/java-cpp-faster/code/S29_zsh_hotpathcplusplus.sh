git clone https://github.com/hotpathcplusplus/hotpathcplusplus.git
cd hotpathcplusplus

bazel run --config=opt //episodes/13_cpp_vs_java_jit:ep13_suite
./episodes/13_cpp_vs_java_jit/run_ep13_forensics.sh
