#pragma once
// Worker thread para trabajo que NO debe correr en el audio thread
// (parsing SF2, I/O de disco, carga de samples). Comunicación con el
// audio thread debe pasar por colas lock-free (fuera de alcance de
// Fase 2; aquí solo se fija el contrato).

#include <functional>

namespace olysf2sampler::threading {

using WorkerTask = std::function<void()>;

class WorkerThread {
public:
    virtual ~WorkerThread() = default;

    virtual void enqueue(WorkerTask task) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

}  // namespace olysf2sampler::threading
