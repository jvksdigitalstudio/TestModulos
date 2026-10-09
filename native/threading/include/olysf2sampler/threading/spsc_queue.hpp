#pragma once
// native/threading/spsc_queue — cola de un solo productor / un solo
// consumidor, lock-free, capacidad fija (sin allocación tras
// construirse). Responsabilidad única: paso de datos seguro entre un
// hilo de control (que empuja comandos) y el audio thread (que los
// consume al inicio de cada bloque), sin locks bloqueantes (Fase 1
// §13 / docs/architecture/THREADING.md).
//
// Implementación estándar de anillo circular SPSC: correcta y
// wait-free SIEMPRE que se respete la restricción de un único
// productor y un único consumidor (no es multi-productor ni
// multi-consumidor — si se necesita eso, hace falta una estructura
// distinta, no esta).

#include <array>
#include <atomic>
#include <cstddef>

namespace olysf2sampler::threading {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "SpscQueue necesita capacidad >= 2");

public:
    /// Intenta encolar `item` (copia). Devuelve false si la cola está
    /// llena (el llamador decide qué hacer — típicamente descartar,
    /// documentado en el punto de uso). Wait-free, sin asignación.
    [[nodiscard]] bool push(const T& item) noexcept {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t nextTail = advance(tail);
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false;  // llena
        }
        buffer_[tail] = item;
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    /// Intenta extraer un elemento en `out`. Devuelve false si la cola
    /// está vacía. Wait-free, sin asignación.
    [[nodiscard]] bool pop(T& out) noexcept {
        std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // vacía
        }
        out = buffer_[head];
        head_.store(advance(head), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] static std::size_t advance(std::size_t index) noexcept {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> buffer_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace olysf2sampler::threading
