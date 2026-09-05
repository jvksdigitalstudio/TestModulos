#pragma once
// native/scheduler — orquestación de tareas asíncronas fuera
// del audio thread (p.ej. "parsear este SF2 y notificar cuando esté
// listo"). Responsabilidad única: planificar, no ejecutar DSP/parsing
// directamente (delega en threading::WorkerThread).

#include <chrono>
#include <functional>

namespace olysf2sampler::scheduler {

using ScheduledTask = std::function<void()>;

class TaskScheduler {
public:
    virtual ~TaskScheduler() = default;

    virtual void submit(ScheduledTask task) = 0;
    virtual void submitDelayed(ScheduledTask task, std::chrono::milliseconds delay) = 0;
};

}  // namespace olysf2sampler::scheduler
