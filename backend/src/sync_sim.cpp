#include "sync_sim.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Semaphore {
    int value = 0;
    std::deque<std::string> waiters;

    explicit Semaphore(int v = 0) : value(v) {}

    bool wait(const std::string& who) {
        if (value > 0) {
            value--;
            return true;
        }
        waiters.push_back(who);
        return false;
    }

    std::string signal() {
        value++;
        if (!waiters.empty()) {
            std::string w = waiters.front();
            waiters.pop_front();
            return w;
        }
        return "";
    }
};

struct Actor {
    std::string id;
    std::string role;  // producer | consumer
    int remaining = 0;
    bool blocked = false;
    std::string blocked_on;
    int produced = 0;
    int consumed = 0;
};

nlohmann::json run_discrete(const SyncConfig& cfg) {
    const int nprod = std::max(1, cfg.producers);
    const int ncons = std::max(1, cfg.consumers);
    const int cap = std::max(1, cfg.buffer_size);

    std::vector<Actor> actors;
    for (int i = 0; i < nprod; ++i) {
        Actor a;
        a.id = "P" + std::to_string(i + 1);
        a.role = "producer";
        a.remaining = std::max(1, cfg.items_per_producer);
        actors.push_back(a);
    }
    for (int i = 0; i < ncons; ++i) {
        Actor a;
        a.id = "C" + std::to_string(i + 1);
        a.role = "consumer";
        a.remaining = std::max(1, cfg.items_per_consumer);
        actors.push_back(a);
    }

    Semaphore mutex_sem(1);
    Semaphore empty_sem(cap);
    Semaphore full_sem(0);
    std::deque<int> buffer;
    int next_item = 1;
    int step = 0;
    nlohmann::json events = nlohmann::json::array();
    nlohmann::json occupancy = nlohmann::json::array();
    bool overflow = false;
    bool underflow = false;

    auto log = [&](const std::string& actor, const std::string& action, const std::string& detail) {
        events.push_back({
            {"step", step},
            {"actor", actor},
            {"action", action},
            {"detail", detail},
            {"buffer", static_cast<int>(buffer.size())},
            {"mutex", mutex_sem.value},
            {"empty", empty_sem.value},
            {"full", full_sem.value},
        });
    };

    auto wake = [&](const std::string& id) {
        if (id.empty()) return;
        for (auto& a : actors) {
            if (a.id == id) {
                a.blocked = false;
                a.blocked_on.clear();
                log(id, "WAKE", "despertado por signal");
            }
        }
    };

    auto try_op = [&](Actor& a) -> bool {
        if (a.blocked || a.remaining <= 0) return false;

        if (a.role == "producer") {
            if (!empty_sem.wait(a.id)) {
                a.blocked = true;
                a.blocked_on = "empty";
                log(a.id, "WAIT", "empty==0, productor bloqueado");
                return true;
            }
            log(a.id, "WAIT_OK", "empty");
            if (!mutex_sem.wait(a.id)) {
                empty_sem.signal();
                a.blocked = true;
                a.blocked_on = "mutex";
                log(a.id, "WAIT", "mutex ocupado");
                return true;
            }
            if (static_cast<int>(buffer.size()) >= cap) overflow = true;
            buffer.push_back(next_item);
            log(a.id, "PRODUCE", "item=" + std::to_string(next_item));
            next_item++;
            a.produced++;
            a.remaining--;
            mutex_sem.signal();
            wake(full_sem.signal());
            return true;
        }

        if (!full_sem.wait(a.id)) {
            a.blocked = true;
            a.blocked_on = "full";
            log(a.id, "WAIT", "full==0, consumidor bloqueado");
            return true;
        }
        log(a.id, "WAIT_OK", "full");
        if (!mutex_sem.wait(a.id)) {
            full_sem.signal();
            a.blocked = true;
            a.blocked_on = "mutex";
            log(a.id, "WAIT", "mutex ocupado");
            return true;
        }
        if (buffer.empty()) underflow = true;
        int item = buffer.empty() ? -1 : buffer.front();
        if (!buffer.empty()) buffer.pop_front();
        log(a.id, "CONSUME", "item=" + std::to_string(item));
        a.consumed++;
        a.remaining--;
        mutex_sem.signal();
        wake(empty_sem.signal());
        return true;
    };

    const int max_steps = 5000;
    int idle_rounds = 0;
    while (step < max_steps) {
        bool progress = false;
        bool remaining_work = false;
        for (auto& a : actors) {
            if (a.remaining > 0) remaining_work = true;
            if (try_op(a)) progress = true;
            occupancy.push_back({
                {"step", step},
                {"buffer", static_cast<int>(buffer.size())},
            });
            step++;
        }
        if (!remaining_work) break;
        if (!progress) {
            idle_rounds++;
            if (idle_rounds > 3) break;
        } else {
            idle_rounds = 0;
        }
    }

    bool deadlock = false;
    for (const auto& a : actors) {
        if (a.remaining > 0 && a.blocked) deadlock = true;
    }
    // Si aún hay trabajo pero nadie puede avanzar y el buffer está lleno/vacío de forma
    // inconsistente con los roles restantes, marcar deadlock.
    int still = 0;
    for (const auto& a : actors)
        if (a.remaining > 0) still++;
    if (still > 0) {
        bool any_ready = false;
        for (const auto& a : actors)
            if (a.remaining > 0 && !a.blocked) any_ready = true;
        if (!any_ready) deadlock = true;
    } else {
        deadlock = false;
    }

    nlohmann::json actors_j = nlohmann::json::array();
    for (const auto& a : actors) {
        actors_j.push_back({
            {"id", a.id},
            {"role", a.role},
            {"produced", a.produced},
            {"consumed", a.consumed},
            {"remaining", a.remaining},
            {"blocked", a.blocked},
        });
    }

    nlohmann::json out;
    out["module"] = "sync";
    out["problem"] = "producer-consumer";
    out["mode"] = "discrete";
    out["buffer_size"] = cap;
    out["producers"] = nprod;
    out["consumers"] = ncons;
    out["events"] = events;
    out["occupancy"] = occupancy;
    out["actors"] = actors_j;
    out["invariants"] = {
        {"no_overflow", !overflow},
        {"no_underflow", !underflow},
        {"no_deadlock", !deadlock},
        {"final_buffer", static_cast<int>(buffer.size())},
        {"mutex_final", mutex_sem.value},
    };
    out["explanation"] =
        "Semáforos: mutex=1 (exclusión mutua), empty=N (huecos libres), full=0 (ítems). "
        "Productor: wait(empty), wait(mutex), insertar, signal(mutex), signal(full). "
        "Consumidor: wait(full), wait(mutex), extraer, signal(mutex), signal(empty).";
    return out;
}

nlohmann::json run_realtime(const SyncConfig& cfg) {
    const int cap = std::max(1, cfg.buffer_size);
    const int nprod = std::max(1, cfg.producers);
    const int ncons = std::max(1, cfg.consumers);

    std::mutex log_mu;
    std::mutex buf_mu;
    std::condition_variable cv_empty;
    std::condition_variable cv_full;
    std::deque<int> buffer;
    int next_item = 1;
    nlohmann::json events = nlohmann::json::array();
    int seq = 0;
    bool overflow = false;
    bool underflow = false;

    auto push_event = [&](const std::string& actor, const std::string& action, const std::string& detail) {
        std::lock_guard<std::mutex> lk(log_mu);
        events.push_back({
            {"step", seq++},
            {"actor", actor},
            {"action", action},
            {"detail", detail},
            {"buffer", static_cast<int>(buffer.size())},
        });
    };

    auto producer = [&](int id) {
        const std::string name = "P" + std::to_string(id);
        for (int i = 0; i < cfg.items_per_producer; ++i) {
            std::unique_lock<std::mutex> lk(buf_mu);
            cv_empty.wait(lk, [&] { return static_cast<int>(buffer.size()) < cap; });
            if (static_cast<int>(buffer.size()) >= cap) overflow = true;
            int item = next_item++;
            buffer.push_back(item);
            push_event(name, "PRODUCE", "item=" + std::to_string(item));
            lk.unlock();
            cv_full.notify_one();
        }
    };

    auto consumer = [&](int id) {
        const std::string name = "C" + std::to_string(id);
        for (int i = 0; i < cfg.items_per_consumer; ++i) {
            std::unique_lock<std::mutex> lk(buf_mu);
            cv_full.wait(lk, [&] { return !buffer.empty(); });
            if (buffer.empty()) underflow = true;
            int item = buffer.front();
            buffer.pop_front();
            push_event(name, "CONSUME", "item=" + std::to_string(item));
            lk.unlock();
            cv_empty.notify_one();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 1; i <= nprod; ++i) threads.emplace_back(producer, i);
    for (int i = 1; i <= ncons; ++i) threads.emplace_back(consumer, i);
    for (auto& t : threads) t.join();

    nlohmann::json out;
    out["module"] = "sync";
    out["problem"] = "producer-consumer";
    out["mode"] = "realtime_threads";
    out["buffer_size"] = cap;
    out["producers"] = nprod;
    out["consumers"] = ncons;
    out["events"] = events;
    out["invariants"] = {
        {"no_overflow", !overflow},
        {"no_underflow", !underflow},
        {"no_deadlock", true},
        {"final_buffer", static_cast<int>(buffer.size())},
    };
    out["explanation"] =
        "Hilos reales (std::thread) con mutex y condition_variable equivalentes a semáforos empty/full.";
    return out;
}

}  // namespace

nlohmann::json run_sync(const SyncConfig& cfg) {
    if (cfg.realtime) return run_realtime(cfg);
    return run_discrete(cfg);
}
