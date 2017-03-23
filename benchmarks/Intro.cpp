// Copyright 2004-present Facebook. All Rights Reserved.

#include <string>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>

#include <benchmark/benchmark.h>

static void BM_BasicWithArg(benchmark::State& state)
{
    while (state.KeepRunning())
    {
        std::string str(state.range(0), 'a');
    }
}

BENCHMARK(BM_BasicWithArg)->Arg(10)->Arg(100);

BENCHMARK(BM_BasicWithArg)->Range(8, 1024);

static void BM_BasicWithArgs(benchmark::State& state)
{
    while (state.KeepRunning())
    {
        std::string str(state.range(0), state.range(1));
    }
}

BENCHMARK(BM_BasicWithArgs)->Args({ 16, 'a' })->Args({ 128, 'b' });

static void BM_CustomThroughput(benchmark::State &state)
{
    unsigned long long numOfCalls = 0;

    while (state.KeepRunning())
    {
        std::this_thread::yield();
        numOfCalls++;
    }

    state.SetItemsProcessed(numOfCalls);
}

BENCHMARK(BM_CustomThroughput);

static void BM_CustomLabel(benchmark::State &state)
{
    unsigned long long numThirds = 0, total = 0;

    std::random_device randomDevice;
    std::default_random_engine randomEngine(randomDevice());
    std::uniform_int_distribution<int> uniformDist(1, 6);

    while (state.KeepRunning())
    {
        if (uniformDist(randomEngine) <= 2)
        {
            numThirds++;
        }

        total++;
    }

    char label[256];

    std::snprintf(label, sizeof(label) - 1, "Percentage: %f%%", 100.0 * ((double)numThirds / (double)total));

    state.SetLabel(label);
}

BENCHMARK(BM_CustomLabel);

BENCHMARK_MAIN();
