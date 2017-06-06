
##########################################################
# Google benchmark


benchmark(baselines Baselines.cpp)
benchmark(streamthroughput StreamThroughput.cpp)
benchmark(reqrespthroughput RequestResponseThroughput.cpp)
benchmark(reqresplatency RequestResponseLatency.cpp)
