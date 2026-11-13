// LISTEN's capture is fed from the audio thread while the message thread starts, stops and clears it. This hammers that boundary:
// 300 start/stop cycles with a thread feeding the whole time. It is most useful under ThreadSanitizer (SANITIZE=thread scripts/run_tests.sh listen_race).
#include "../Source/Offline/ListenSession.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

int main()
{
    choir::ListenSession session; std::atomic<bool> run { true }; std::atomic<long> blocks { 0 }; std::vector<float> block(512, .1f);
    std::thread audio([&] { double t = 0; while (run) { session.feed(block.data(), 512, t, true); t += 512 / 44100.0; blocks.fetch_add(1); std::this_thread::yield(); } });
    int ready = 0;
    for (int i = 0; i < 300; ++i)
    {
        if (!session.start(44100, 3.0)) { std::printf("FAIL: start refused in state %d\n", (int) session.state()); std::exit(1); }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        session.stop(0, 0, 0);
        for (int k = 0; k < 4000 && session.state() != choir::ListenSession::Ready; ++k) std::this_thread::sleep_for(std::chrono::microseconds(500));
        if (session.state() != choir::ListenSession::Ready) { std::printf("FAIL: the analysis did not finish (cycle %d)\n", i); std::exit(1); }
        ++ready;
        if (i % 7 == 0) session.clear();
    }
    run = false; audio.join();
    if (blocks.load() < 100) { std::printf("FAIL: the audio thread only ran %ld blocks\n", blocks.load()); return 1; }
    std::printf("%d start/stop/analyse cycles with the audio thread feeding throughout (%ld blocks)\nALL LISTEN RACE TESTS PASSED\n", ready, blocks.load());
}
