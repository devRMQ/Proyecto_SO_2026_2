#include "os_sim.hpp"

#include <algorithm>
#include <climits>
#include <deque>
#include <map>
#include <sstream>

namespace {

using json = nlohmann::json;

enum class St { NEW, JOB_WAIT, READY, RUNNING, BLOCKED_PF, BLOCKED_SEM, TERMINATED, REJECTED };

const char* st_str(St s) {
    switch (s) {
        case St::NEW: return "NEW";
        case St::JOB_WAIT: return "JOB_WAIT";
        case St::READY: return "READY";
        case St::RUNNING: return "RUNNING";
        case St::BLOCKED_PF: return "BLOCKED_PF";
        case St::BLOCKED_SEM: return "BLOCKED_SEM";
        case St::TERMINATED: return "TERMINATED";
        case St::REJECTED: return "REJECTED";
    }
    return "?";
}

const char* role_str(ProcRole r) {
    switch (r) {
        case ProcRole::PRODUCER: return "producer";
        case ProcRole::CONSUMER: return "consumer";
        default: return "normal";
    }
}

template <typename T>
T jget(const json& j, const char* key, T def) {
    if (j.is_object() && j.contains(key) && !j[key].is_null()) return j[key].get<T>();
    return def;
}

// ---------------------------------------------------------------- PCB
struct Proc {
    OsProcess d;
    St st = St::NEW;
    int remaining = 0;
    int quantum_used = 0;
    int first_run = -1;
    int finish = -1;
    int ready_ticks = 0, pf_ticks = 0, sem_ticks = 0, mem_ticks = 0;
    int faults = 0, mem_refs = 0;
    int refs_done = 0;      // índice dentro de su cadena de referencias
    int pf_timer = 0;
    bool granted = false;   // semáforo concedido por signal() (handoff)
    std::string blocked_on;
    std::vector<int> refs;
};

// Generador determinista con localidad (si el escenario no trae referencias).
std::vector<int> gen_refs(int pid, int pages, int n) {
    std::vector<int> out;
    unsigned long long s = 1103515245ULL * (unsigned long long)(pid + 7) + 12345ULL;
    auto rnd = [&]() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return (int)((s >> 33) & 0x7fffffff);
    };
    int cur = 0;
    int w = std::max(1, pages / 2);
    int base = 0;
    for (int i = 0; i < n; ++i) {
        int r = rnd() % 100;
        if (r < 55) cur = cur + (rnd() % 2);
        else if (r < 85) cur = base + rnd() % w;
        else cur = rnd() % pages;
        if (i % 12 == 11) base = rnd() % pages;
        cur = ((cur % pages) + pages) % pages;
        out.push_back(cur);
    }
    return out;
}

// ---------------------------------------------------------------- memoria contigua
struct Block {
    int start, size, pid;  // pid -1 = hueco
};

struct ContigMem {
    int total = 0;
    AllocStrategy strat = AllocStrategy::FIRST_FIT;
    std::vector<Block> blocks;

    void init(int t, AllocStrategy s) {
        total = t;
        strat = s;
        blocks = {{0, t, -1}};
    }
    bool alloc(int pid, int size) {
        int pick = -1;
        for (int i = 0; i < (int)blocks.size(); ++i) {
            if (blocks[i].pid != -1 || blocks[i].size < size) continue;
            if (pick == -1) pick = i;
            else if (strat == AllocStrategy::BEST_FIT && blocks[i].size < blocks[pick].size) pick = i;
            else if (strat == AllocStrategy::WORST_FIT && blocks[i].size > blocks[pick].size) pick = i;
            if (strat == AllocStrategy::FIRST_FIT) break;
        }
        if (pick == -1) return false;
        Block b = blocks[pick];
        blocks[pick] = {b.start, size, pid};
        if (b.size > size) blocks.insert(blocks.begin() + pick + 1, {b.start + size, b.size - size, -1});
        return true;
    }
    void release(int pid) {
        for (auto& b : blocks)
            if (b.pid == pid) b.pid = -1;
        std::vector<Block> m;
        for (const auto& b : blocks) {
            if (!m.empty() && m.back().pid == -1 && b.pid == -1) m.back().size += b.size;
            else m.push_back(b);
        }
        blocks = m;
    }
    int free_total() const {
        int f = 0;
        for (auto& b : blocks) if (b.pid == -1) f += b.size;
        return f;
    }
    int largest() const {
        int l = 0;
        for (auto& b : blocks) if (b.pid == -1) l = std::max(l, b.size);
        return l;
    }
    int holes() const {
        int h = 0;
        for (auto& b : blocks) if (b.pid == -1) ++h;
        return h;
    }
};

// ---------------------------------------------------------------- memoria paginada
struct Frame {
    int pid = -1, page = -1;
    int loaded_at = 0, last_used = 0;
    bool loading = false;
    bool pinned = false;  // recién cargada: no se expulsa hasta que su dueño la use una vez
};

// ---------------------------------------------------------------- semáforos
struct Sem {
    int value = 0;
    std::deque<int> q;
};

struct Engine {
    const OsConfig& cfg;
    bool record;
    std::vector<Proc> P;
    std::deque<int> ready;     // índices en P
    std::deque<int> job_q;
    int running = -1;
    int last_dispatched = -1;
    int t = 0;

    ContigMem cm;
    std::vector<Frame> fr;
    int total_faults = 0, total_refs = 0;

    Sem mutex, empty_s, full_s;
    int buffer = 0, produced = 0, consumed = 0, max_occ = 0, overflow = 0, underflow = 0;
    bool sync_on = false;
    bool deadlock = false;

    int ctx_switches = 0, cpu_busy = 0;
    long long frag_sum = 0;
    int frag_samples = 0;
    std::vector<std::string> tick_events;
    json log = json::array();
    json frames = json::array();
    std::vector<int> cpu_trace;  // pid que usó la CPU en cada tick (-1 = ociosa)

    Engine(const OsConfig& c, bool rec) : cfg(c), record(rec) {}

    void ev(const std::string& s) {
        tick_events.push_back(s);
        if (record) log.push_back({{"tick", t}, {"text", s}});
    }
    std::string nm(int i) const { return P[i].d.name + "(" + std::to_string(P[i].d.pid) + ")"; }

    void setup() {
        for (const auto& d : cfg.processes) {
            Proc p;
            p.d = d;
            p.d.burst = std::max(1, d.burst);
            p.d.pages = std::max(1, d.pages);
            p.remaining = p.d.burst;
            if (!d.refs.empty()) {
                for (int r : d.refs) p.refs.push_back(((r % p.d.pages) + p.d.pages) % p.d.pages);
            } else {
                p.refs = gen_refs(d.pid, p.d.pages, p.d.burst);
            }
            if (d.role != ProcRole::NORMAL) sync_on = true;
            P.push_back(p);
        }
        if (cfg.mem_mode == MemMode::CONTIGUOUS) cm.init(cfg.total_memory, cfg.strategy);
        else fr.assign(std::max(1, cfg.frames), Frame());
        empty_s.value = std::max(1, cfg.buffer_size);
        full_s.value = 0;
        mutex.value = 1;
    }

    // ------------------------------------------------ memoria
    void free_memory(int i) {
        if (cfg.mem_mode == MemMode::CONTIGUOUS) cm.release(P[i].d.pid);
        else for (auto& f : fr) if (f.pid == P[i].d.pid) f = Frame();
    }

    void make_ready(int i) {
        P[i].st = St::READY;
        ready.push_back(i);
    }

    void admit(int i) {
        Proc& p = P[i];
        if (cfg.mem_mode == MemMode::PAGED) {
            ev(nm(i) + " llega y pasa a READY (" + std::to_string(p.d.pages) + " páginas virtuales)");
            make_ready(i);
            return;
        }
        if (p.d.mem_size > cfg.total_memory) {
            p.st = St::REJECTED;
            p.finish = t;
            ev(nm(i) + " RECHAZADO: pide " + std::to_string(p.d.mem_size) + " > memoria total " +
               std::to_string(cfg.total_memory));
            return;
        }
        if (cm.alloc(p.d.pid, p.d.mem_size)) {
            ev(nm(i) + " obtiene " + std::to_string(p.d.mem_size) + " u. de memoria → READY");
            make_ready(i);
        } else {
            p.st = St::JOB_WAIT;
            job_q.push_back(i);
            bool frag = cm.free_total() >= p.d.mem_size;
            ev(nm(i) + " espera memoria" +
               (frag ? " (FRAGMENTACIÓN EXTERNA: hay espacio libre pero no contiguo)" : " (memoria insuficiente)"));
        }
    }

    void retry_jobs() {
        for (auto it = job_q.begin(); it != job_q.end();) {
            int i = *it;
            if (cm.alloc(P[i].d.pid, P[i].d.mem_size)) {
                ev(nm(i) + " sale de la cola de trabajos: memoria asignada → READY");
                make_ready(i);
                it = job_q.erase(it);
            } else ++it;
        }
    }

    int next_use_distance(const Proc& q, int page) const {
        int n = (int)q.refs.size();
        for (int k = 0; k < q.remaining; ++k)
            if (q.refs[(q.refs_done + k) % n] == page) return k;
        return INT_MAX;
    }

    // devuelve: 0 acierto, 1 fallo (frame asignado, carga en curso), 2 sin víctima (reintentar)
    int access(int i, int page) {
        Proc& p = P[i];
        p.mem_refs++;
        total_refs++;
        for (auto& f : fr)
            if (f.pid == p.d.pid && f.page == page && !f.loading) {
                f.last_used = t;
                f.pinned = false;
                return 0;
            }
        int victim = -1;
        for (int k = 0; k < (int)fr.size(); ++k)
            if (fr[k].pid == -1) { victim = k; break; }
        std::string how = "marco libre";
        if (victim == -1) {
            long best = -1;
            for (int k = 0; k < (int)fr.size(); ++k) {
                if (fr[k].loading || fr[k].pinned) continue;
                long score;
                if (cfg.replacement == ReplaceAlgo::FIFO) score = -(long)fr[k].loaded_at;
                else if (cfg.replacement == ReplaceAlgo::LRU) score = -(long)fr[k].last_used;
                else {
                    int owner = -1;
                    for (int j = 0; j < (int)P.size(); ++j) if (P[j].d.pid == fr[k].pid) owner = j;
                    int dist = owner < 0 ? INT_MAX : next_use_distance(P[owner], fr[k].page);
                    score = (long)dist * 100000L - fr[k].loaded_at;  // empate → el más antiguo
                }
                if (victim == -1 || score > best) { best = score; victim = k; }
            }
            if (victim == -1) return 2;
            how = "víctima " + replaceToString(cfg.replacement) + ": pág " + std::to_string(fr[victim].page) +
                  " de PID " + std::to_string(fr[victim].pid);
        }
        fr[victim] = {p.d.pid, page, t, t, true, true};
        p.faults++;
        total_faults++;
        ev("FALLO DE PÁGINA: " + nm(i) + " pág " + std::to_string(page) + " → marco " + std::to_string(victim) +
           " (" + how + ")");
        return 1;
    }

    // ------------------------------------------------ planificador
    int pick_ready() {
        if (ready.empty()) return -1;
        auto better = [&](int a, int b) {  // ¿a es estrictamente mejor que b?
            if (cfg.algorithm == SchedulerAlgo::SJF) return P[a].remaining < P[b].remaining;
            if (cfg.algorithm == SchedulerAlgo::PRIORITY) return P[a].d.priority > P[b].d.priority;
            return false;
        };
        auto best = ready.begin();
        for (auto it = ready.begin(); it != ready.end(); ++it)
            if (better(*it, *best)) best = it;
        int idx = *best;
        ready.erase(best);
        return idx;
    }

    bool should_preempt() {
        if (running < 0 || !cfg.preemptive) return false;
        for (int i : ready) {
            if (cfg.algorithm == SchedulerAlgo::SJF && P[i].remaining < P[running].remaining) return true;
            if (cfg.algorithm == SchedulerAlgo::PRIORITY && P[i].d.priority > P[running].d.priority) return true;
        }
        return false;
    }

    void wake(Sem& s, const char* name) {
        if (s.q.empty()) { s.value++; return; }
        int w = s.q.front();
        s.q.pop_front();
        P[w].granted = true;
        P[w].blocked_on.clear();
        ev("signal(" + std::string(name) + ") despierta a " + nm(w));
        make_ready(w);
    }

    // Ejecuta el proceso en CPU durante este tick. Devuelve true si consumió el tick.
    bool execute(int i) {
        Proc& p = P[i];
        if (cfg.mem_mode == MemMode::PAGED) {
            int page = p.refs[p.refs_done % p.refs.size()];
            int r = access(i, page);
            if (r != 0) {
                p.st = St::BLOCKED_PF;
                p.pf_timer = (r == 1) ? std::max(1, cfg.pf_penalty) : 1;
                running = -1;
                return true;  // el tick se gasta en el trap del fallo
            }
        }
        if (p.d.role != ProcRole::NORMAL) {
            bool prod = p.d.role == ProcRole::PRODUCER;
            Sem& first = prod ? empty_s : full_s;
            const char* fname = prod ? "empty" : "full";
            if (p.granted) p.granted = false;
            else if (first.value > 0) first.value--;
            else {
                first.q.push_back(i);
                p.st = St::BLOCKED_SEM;
                p.blocked_on = fname;
                ev("wait(" + std::string(fname) + ") bloquea a " + nm(i) +
                   (prod ? " — buffer LLENO" : " — buffer VACÍO"));
                running = -1;
                return false;  // no consume CPU: se despacha otro en el mismo tick
            }
            mutex.value = 0;
            if (prod) {
                buffer++;
                produced++;
                if (buffer > cfg.buffer_size) overflow++;
                max_occ = std::max(max_occ, buffer);
                ev(nm(i) + " [wait(empty) wait(mutex)] produce ítem #" + std::to_string(produced) +
                   " (buffer " + std::to_string(buffer) + "/" + std::to_string(cfg.buffer_size) + ")");
            } else {
                buffer--;
                consumed++;
                if (buffer < 0) underflow++;
                ev(nm(i) + " [wait(full) wait(mutex)] consume ítem (buffer " + std::to_string(buffer) + "/" +
                   std::to_string(cfg.buffer_size) + ")");
            }
            mutex.value = 1;
            wake(prod ? full_s : empty_s, prod ? "full" : "empty");
        }
        p.remaining--;
        p.refs_done++;
        p.quantum_used++;
        if (p.remaining == 0) {
            p.st = St::TERMINATED;
            p.finish = t + 1;
            ev(nm(i) + " TERMINA");
            free_memory(i);
            if (cfg.mem_mode == MemMode::CONTIGUOUS) {
                retry_jobs();
            }
            running = -1;
        }
        return true;
    }

    bool all_done() const {
        for (const auto& p : P)
            if (p.st != St::TERMINATED && p.st != St::REJECTED) return false;
        return true;
    }

    json snapshot(int cpu_pid) const {
        json f;
        f["tick"] = t;
        f["cpu"] = cpu_pid;
        f["running"] = running >= 0 ? P[running].d.pid : -1;
        json rq = json::array(), pf = json::array(), sm = json::array(), jq = json::array();
        for (int i : ready) rq.push_back(P[i].d.pid);
        for (const auto& p : P) {
            if (p.st == St::BLOCKED_PF) pf.push_back(p.d.pid);
            if (p.st == St::BLOCKED_SEM) sm.push_back({{"pid", p.d.pid}, {"sem", p.blocked_on}});
        }
        for (int i : job_q) jq.push_back(P[i].d.pid);
        f["ready"] = rq;
        f["blocked_pf"] = pf;
        f["blocked_sem"] = sm;
        f["job_queue"] = jq;
        json ps = json::array();
        for (const auto& p : P)
            ps.push_back({{"pid", p.d.pid}, {"state", st_str(p.st)}, {"remaining", p.remaining},
                          {"faults", p.faults}});
        f["procs"] = ps;
        json m;
        if (cfg.mem_mode == MemMode::CONTIGUOUS) {
            json bl = json::array();
            for (const auto& b : cm.blocks)
                bl.push_back({{"start", b.start}, {"size", b.size}, {"pid", b.pid}});
            m = {{"mode", "contiguous"}, {"total", cm.total}, {"blocks", bl}, {"free", cm.free_total()},
                 {"largest_hole", cm.largest()}, {"holes", cm.holes()}};
        } else {
            json fl = json::array();
            for (int k = 0; k < (int)fr.size(); ++k)
                fl.push_back({{"frame", k}, {"pid", fr[k].pid}, {"page", fr[k].page}, {"loading", fr[k].loading}});
            m = {{"mode", "paged"}, {"frames", fl}, {"faults", total_faults}, {"refs", total_refs}};
        }
        f["memory"] = m;
        if (sync_on)
            f["sync"] = {{"mutex", mutex.value}, {"empty", empty_s.value}, {"full", full_s.value},
                         {"buffer", buffer}, {"capacity", cfg.buffer_size}};
        f["events"] = tick_events;
        return f;
    }

    void run() {
        setup();
        for (t = 0; t < cfg.max_ticks; ++t) {
            tick_events.clear();
            // 1. llegadas
            for (int i = 0; i < (int)P.size(); ++i)
                if (P[i].st == St::NEW && P[i].d.arrival <= t) admit(i);
            // 2. cola de trabajos (memoria contigua)
            if (cfg.mem_mode == MemMode::CONTIGUOUS && !job_q.empty()) retry_jobs();
            // 3. temporizadores de fallo de página
            for (int i = 0; i < (int)P.size(); ++i)
                if (P[i].st == St::BLOCKED_PF && --P[i].pf_timer <= 0) {
                    for (auto& f : fr) if (f.pid == P[i].d.pid && f.loading) f.loading = false;
                    ev(nm(i) + ": página cargada → READY");
                    make_ready(i);
                }
            // 4. expropiación
            if (should_preempt()) {
                ev(nm(running) + " expropiado");
                P[running].st = St::READY;
                ready.push_back(running);
                running = -1;
            }
            // 5. estado al inicio del tick (para la contabilidad de esperas)
            std::vector<St> start_st;
            for (const auto& p : P) start_st.push_back(p.st);
            // 6. despacho + ejecución (si un proceso se bloquea en un semáforo, se despacha otro)
            int cpu_pid = -1;
            for (int guard = 0; guard <= (int)P.size() + 1; ++guard) {
                if (running < 0) {
                    int n = pick_ready();
                    if (n < 0) break;
                    running = n;
                    P[n].st = St::RUNNING;
                    P[n].quantum_used = 0;
                    if (P[n].first_run < 0) P[n].first_run = t;
                    if (last_dispatched != n) {
                        ctx_switches++;
                        ev("DISPATCH → " + nm(n));
                    }
                    last_dispatched = n;
                }
                int cur = running;
                bool used = execute(cur);
                if (used) { cpu_pid = P[cur].d.pid; break; }
            }
            // 7. contabilidad de esperas: el proceso que usó la CPU no espera en este tick
            for (int i = 0; i < (int)P.size(); ++i) {
                if (P[i].d.pid == cpu_pid) continue;
                if (start_st[i] == St::READY) P[i].ready_ticks++;
                else if (start_st[i] == St::BLOCKED_PF) P[i].pf_ticks++;
                else if (start_st[i] == St::BLOCKED_SEM) P[i].sem_ticks++;
                else if (start_st[i] == St::JOB_WAIT) P[i].mem_ticks++;
            }
            // 8. fin de quantum (RR)
            if (running >= 0 && cfg.algorithm == SchedulerAlgo::RR && P[running].quantum_used >= cfg.quantum) {
                ev(nm(running) + " agota su quantum (" + std::to_string(cfg.quantum) + ") → fin de la cola");
                P[running].st = St::READY;
                ready.push_back(running);
                running = -1;
            }
            if (cpu_pid >= 0) cpu_busy++;
            cpu_trace.push_back(cpu_pid);
            if (cfg.mem_mode == MemMode::CONTIGUOUS) {
                int fr_ext = cm.free_total() - cm.largest();
                frag_sum += fr_ext;
                frag_samples++;
            }
            // 9. interbloqueo
            bool done = all_done();
            if (!done && running < 0 && ready.empty()) {
                bool future = false, timers = false, blocked = false;
                for (const auto& p : P) {
                    if (p.st == St::NEW) future = true;
                    if (p.st == St::BLOCKED_PF) timers = true;
                    if (p.st == St::BLOCKED_SEM) blocked = true;
                }
                if (!future && !timers && (blocked || !job_q.empty())) {
                    deadlock = true;
                    ev("INTERBLOQUEO: ningún proceso puede continuar");
                    done = true;
                }
            }
            if (record) frames.push_back(snapshot(cpu_pid));
            if (done) { ++t; break; }
        }
    }
};

json metrics_of(const Engine& e) {
    double aw = 0, ar = 0, at = 0;
    int n = 0;
    for (const auto& p : e.P) {
        if (p.st != St::TERMINATED) continue;
        int turn = p.finish - p.d.arrival;
        aw += p.ready_ticks;
        ar += p.first_run - p.d.arrival;
        at += turn;
        ++n;
    }
    json m;
    m["avg_waiting"] = n ? aw / n : 0.0;
    m["avg_response"] = n ? ar / n : 0.0;
    m["avg_turnaround"] = n ? at / n : 0.0;
    m["total_ticks"] = e.t;
    m["cpu_utilization"] = e.t ? 100.0 * e.cpu_busy / e.t : 0.0;
    m["throughput"] = e.t ? (double)n / e.t : 0.0;
    m["context_switches"] = e.ctx_switches;
    m["completed"] = n;
    m["page_faults"] = e.total_faults;
    m["mem_refs"] = e.total_refs;
    m["fault_rate"] = e.total_refs ? 100.0 * e.total_faults / e.total_refs : 0.0;
    return m;
}

json simulate(const OsConfig& cfg, bool record) {
    Engine e(cfg, record);
    e.run();

    json out;
    out["module"] = "os";
    out["config"] = {
        {"scheduler", algoToString(cfg.algorithm)}, {"quantum", cfg.quantum}, {"preemptive", cfg.preemptive},
        {"mem_mode", cfg.mem_mode == MemMode::PAGED ? "paged" : "contiguous"},
        {"total_memory", cfg.total_memory}, {"strategy", strategyToString(cfg.strategy)},
        {"frames", cfg.frames}, {"replacement", replaceToString(cfg.replacement)},
        {"pf_penalty", cfg.pf_penalty}, {"buffer_size", cfg.buffer_size}};
    out["metrics"] = metrics_of(e);
    out["deadlock"] = e.deadlock;
    out["timeout"] = !e.all_done() && !e.deadlock;

    json procs = json::array();
    for (const auto& p : e.P) {
        int turn = p.finish >= 0 ? p.finish - p.d.arrival : 0;
        procs.push_back({{"pid", p.d.pid}, {"name", p.d.name}, {"arrival", p.d.arrival}, {"burst", p.d.burst},
                         {"priority", p.d.priority}, {"threads", p.d.threads}, {"parent_pid", p.d.parent_pid},
                         {"role", role_str(p.d.role)}, {"mem_size", p.d.mem_size}, {"pages", p.d.pages},
                         {"state", st_str(p.st)}, {"start", p.first_run}, {"finish", p.finish},
                         {"waiting", p.ready_ticks}, {"response", p.first_run >= 0 ? p.first_run - p.d.arrival : -1},
                         {"turnaround", turn}, {"blocked_pf", p.pf_ticks}, {"blocked_sem", p.sem_ticks},
                         {"mem_wait", p.mem_ticks}, {"page_faults", p.faults}, {"mem_refs", p.mem_refs}});
    }
    out["processes"] = procs;

    json gantt = json::array();
    int s = 0;
    for (int k = 1; k <= (int)e.cpu_trace.size(); ++k) {
        if (k == (int)e.cpu_trace.size() || e.cpu_trace[k] != e.cpu_trace[s]) {
            int pid = e.cpu_trace[s];
            std::string name = "idle";
            for (const auto& p : e.P) if (p.d.pid == pid) name = p.d.name;
            gantt.push_back({{"start", s}, {"end", k}, {"pid", pid}, {"name", name}});
            s = k;
        }
    }
    out["gantt"] = gantt;

    if (cfg.mem_mode == MemMode::PAGED) {
        out["memory"] = {{"mode", "paged"}, {"frames", cfg.frames}, {"replacement", replaceToString(cfg.replacement)},
                         {"page_faults", e.total_faults}, {"references", e.total_refs},
                         {"hits", e.total_refs - e.total_faults},
                         {"fault_rate", e.total_refs ? 100.0 * e.total_faults / e.total_refs : 0.0}};
    } else {
        int rejected = 0, waited = 0;
        for (const auto& p : e.P) {
            if (p.st == St::REJECTED) ++rejected;
            if (p.mem_ticks > 0) ++waited;
        }
        out["memory"] = {{"mode", "contiguous"}, {"total", cfg.total_memory},
                         {"strategy", strategyToString(cfg.strategy)}, {"rejected", rejected},
                         {"processes_that_waited", waited},
                         {"avg_external_fragmentation",
                          e.frag_samples ? (double)e.frag_sum / e.frag_samples : 0.0},
                         {"final_free", e.cm.free_total()}, {"final_holes", e.cm.holes()},
                         {"final_largest_hole", e.cm.largest()}};
    }

    json sy = {{"enabled", e.sync_on}};
    if (e.sync_on) {
        bool inv = e.overflow == 0 && e.underflow == 0 && !e.deadlock &&
                   e.empty_s.value + e.buffer == cfg.buffer_size && e.full_s.value == e.buffer;
        sy = {{"enabled", true}, {"buffer_size", cfg.buffer_size}, {"produced", e.produced},
              {"consumed", e.consumed}, {"max_occupancy", e.max_occ}, {"overflow", e.overflow},
              {"underflow", e.underflow}, {"deadlock", e.deadlock}, {"invariants_ok", inv},
              {"final", {{"mutex", e.mutex.value}, {"empty", e.empty_s.value}, {"full", e.full_s.value},
                         {"buffer", e.buffer}}}};
    }
    out["sync"] = sy;

    if (record) {
        out["frames"] = e.frames;
        out["log"] = e.log;
    }
    return out;
}

}  // namespace

OsConfig parse_os_section(const json& j) {
    OsConfig c;
    json sch = j.contains("scheduler") ? j["scheduler"] : json::object();
    c.algorithm = parseAlgo(jget<std::string>(sch, "algorithm", "RR"));
    c.quantum = std::max(1, jget<int>(sch, "quantum", 3));
    c.preemptive = jget<bool>(sch, "preemptive", false);
    json mem = j.contains("memory") ? j["memory"] : json::object();
    c.mem_mode = jget<std::string>(mem, "mode", "paged") == "contiguous" ? MemMode::CONTIGUOUS : MemMode::PAGED;
    c.total_memory = std::max(1, jget<int>(mem, "total_memory", 64));
    c.strategy = parseStrategy(jget<std::string>(mem, "strategy", "FIRST_FIT"));
    c.frames = std::max(1, jget<int>(mem, "frames", 6));
    c.replacement = parseReplace(jget<std::string>(mem, "replacement", "LRU"));
    c.pf_penalty = std::max(1, jget<int>(mem, "page_fault_penalty", 3));
    json sy = j.contains("sync") ? j["sync"] : json::object();
    c.buffer_size = std::max(1, jget<int>(sy, "buffer_size", 3));
    c.max_ticks = std::max(1, jget<int>(j, "max_ticks", 3000));
    c.compare = jget<bool>(j, "compare", false);
    if (j.contains("processes") && j["processes"].is_array()) {
        int auto_pid = 1;
        for (const auto& p : j["processes"]) {
            OsProcess d;
            d.pid = jget<int>(p, "pid", auto_pid);
            d.name = jget<std::string>(p, "name", "P" + std::to_string(d.pid));
            d.arrival = jget<int>(p, "arrival", 0);
            d.burst = jget<int>(p, "burst", 1);
            d.priority = jget<int>(p, "priority", 1);
            d.threads = jget<int>(p, "threads", 1);
            d.parent_pid = jget<int>(p, "parent_pid", -1);
            d.mem_size = jget<int>(p, "mem_size", 8);
            d.pages = jget<int>(p, "pages", 4);
            std::string role = jget<std::string>(p, "role", "normal");
            d.role = role == "producer" ? ProcRole::PRODUCER : role == "consumer" ? ProcRole::CONSUMER : ProcRole::NORMAL;
            if (p.contains("refs") && p["refs"].is_array())
                for (const auto& r : p["refs"]) d.refs.push_back(r.get<int>());
            c.processes.push_back(d);
            auto_pid = std::max(auto_pid, d.pid) + 1;
        }
    }
    return c;
}

nlohmann::json run_os(const OsConfig& cfg) {
    json out = simulate(cfg, true);
    if (cfg.compare) {
        json cmp;
        json sch = json::array();
        for (SchedulerAlgo a : {SchedulerAlgo::FCFS, SchedulerAlgo::SJF, SchedulerAlgo::RR, SchedulerAlgo::PRIORITY}) {
            OsConfig c = cfg;
            c.algorithm = a;
            json r = simulate(c, false);
            json m = r["metrics"];
            m["algorithm"] = algoToString(a);
            sch.push_back(m);
        }
        cmp["schedulers"] = sch;
        if (cfg.mem_mode == MemMode::PAGED) {
            json rep = json::array();
            for (ReplaceAlgo a : {ReplaceAlgo::FIFO, ReplaceAlgo::LRU, ReplaceAlgo::OPTIMAL}) {
                OsConfig c = cfg;
                c.replacement = a;
                json r = simulate(c, false);
                json m = r["metrics"];
                m["algorithm"] = replaceToString(a);
                rep.push_back(m);
            }
            cmp["replacement"] = rep;
        } else {
            json st = json::array();
            for (AllocStrategy a : {AllocStrategy::FIRST_FIT, AllocStrategy::BEST_FIT, AllocStrategy::WORST_FIT}) {
                OsConfig c = cfg;
                c.strategy = a;
                json r = simulate(c, false);
                json m = r["metrics"];
                m["algorithm"] = strategyToString(a);
                m["avg_external_fragmentation"] = r["memory"]["avg_external_fragmentation"];
                st.push_back(m);
            }
            cmp["allocation"] = st;
        }
        out["comparison"] = cmp;
    }
    return out;
}
