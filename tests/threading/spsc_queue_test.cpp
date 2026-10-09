// Test de concurrencia REAL: un hilo productor empuja N enteros
// consecutivos, un hilo consumidor los extrae; se verifica que todos
// llegan, en orden, sin duplicados ni pérdidas. Se ejecuta bajo
// ThreadSanitizer en CI para detectar data races reales, no solo
// "parece funcionar en mi máquina".

#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

#include "olysf2sampler/threading/spsc_queue.hpp"

using olysf2sampler::threading::SpscQueue;

namespace {

constexpr int kItemCount = 200000;

void testConcurrentProducerConsumer() {
    SpscQueue<int, 256> queue;
    std::atomic<bool> producerDone{false};

    std::thread producer([&]() {
        for (int i = 0; i < kItemCount; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();  // cola llena: reintenta
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    int expected = 0;
    int received = 0;
    while (received < kItemCount) {
        int value;
        if (queue.pop(value)) {
            assert(value == expected);  // orden estricto FIFO
            ++expected;
            ++received;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();
    assert(producerDone.load());
    assert(received == kItemCount);
    std::printf("[tests/threading] SPSC queue: %d items, orden correcto, sin pérdidas OK\n",
                kItemCount);
}

void testEmptyAndFullBehavior() {
    SpscQueue<int, 4> queue;
    int dummy;
    assert(!queue.pop(dummy));  // vacía al inicio

    assert(queue.push(1));
    assert(queue.push(2));
    assert(queue.push(3));
    // Capacidad 4 con 1 slot sacrificado para distinguir lleno/vacío
    // (diseño estándar de anillo circular): caben 3 elementos.
    assert(!queue.push(4));  // llena

    int v;
    assert(queue.pop(v) && v == 1);
    assert(queue.push(4));  // ahora hay espacio de nuevo

    std::printf("[tests/threading] SPSC queue: comportamiento lleno/vacío correcto OK\n");
}

}  // namespace

int main() {
    testEmptyAndFullBehavior();
    testConcurrentProducerConsumer();
    std::printf("[tests/threading] TODOS los tests pasaron\n");
    return 0;
}
