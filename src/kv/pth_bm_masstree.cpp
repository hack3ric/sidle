#include "kv/pth_bm_masstree.h"

#include <assert.h>

#include <atomic>
#include <memory>
#include <unordered_map>

#include "helper.h"
#include "kv/masstree.h"
#include "timestamp.hh"

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;
kvepoch_t global_log_epoch = 0;
kvtimestamp_t initial_timestamp = 0;

namespace {

struct PthBmKey {
  uint64_t key{0};

  PthBmKey() = default;
  explicit PthBmKey(uint64_t key_in) : key(key_in) {}

  PthBmKey to_str_key() const {
    if (!little_endian()) {
      return *this;
    }

    PthBmKey reversed;
    for (size_t byte_i = 0; byte_i < sizeof(uint64_t); ++byte_i) {
      reinterpret_cast<char *>(&reversed)[byte_i] =
          reinterpret_cast<const char *>(this)[sizeof(uint64_t) - byte_i - 1];
    }
    return reversed;
  }

  PthBmKey to_normal_key() const { return to_str_key(); }
} PACKED;

struct PthBmValue {
  int64_t val{0};
} PACKED;

using PthBmTree = MasstreeKV<PthBmKey, PthBmValue>;

struct ThreadContext {
  explicit ThreadContext(uint32_t worker_id_in)
      : ti(threadinfo::make(threadinfo::TI_PROCESS,
                            static_cast<int>(worker_id_in))),
        worker_id(worker_id_in) {}

  threadinfo *ti;
  query<row_type> query;
  uint32_t worker_id;
  uint64_t operation_count{0};
};

class PthBmTarget {
 public:
  explicit PthBmTarget(const pth_bm_target_config &config)
      : config_(normalize(config)),
        main_ti_(threadinfo::make(threadinfo::TI_MAIN, -1)),
        tree_(std::make_unique<PthBmTree>(
            main_ti_, config_.cxl_percentage,
            config_.max_local_memory_usage_mib)) {
    if (initial_timestamp == 0) {
      initial_timestamp = timestamp();
    }

    preload();
  }

  ThreadContext &init_thread() {
    auto [it, inserted] =
        g_thread_contexts_.try_emplace(this, next_thread_id_.fetch_add(1));
    if (inserted) {
      it->second.ti->rcu_start();
    }
    return it->second;
  }

  void read(uint64_t key) {
    ThreadContext &ctx = init_thread();
    advance_epoch(ctx);
    PthBmValue value;
    (void)tree_->get(PthBmKey(key), value, ctx.ti, ctx.query, ctx.worker_id);
  }

  void insert(uint64_t key) {
    ThreadContext &ctx = init_thread();
    advance_epoch(ctx);
    bool inserted = tree_->insert(PthBmKey(key), make_value(0xdeadbeefULL),
                                  ctx.ti, ctx.query, ctx.worker_id);
    assert(inserted);
  }

  void update(uint64_t key) {
    ThreadContext &ctx = init_thread();
    advance_epoch(ctx);
    bool updated = tree_->insert(PthBmKey(key), make_value(0xdeadcafeULL),
                                 ctx.ti, ctx.query, ctx.worker_id);
    assert(updated);
  }

  void remove(uint64_t key) {
    ThreadContext &ctx = init_thread();
    advance_epoch(ctx);
    bool removed =
        tree_->remove(PthBmKey(key), ctx.ti, ctx.query, ctx.worker_id);
    assert(removed);
  }

 private:
  static pth_bm_target_config normalize(pth_bm_target_config config) {
    if (config.cxl_percentage < 0) {
      config.cxl_percentage = 0;
    }
    if (config.cxl_percentage > 100) {
      config.cxl_percentage = 100;
    }
    if (config.max_local_memory_usage_mib == 0) {
      config.max_local_memory_usage_mib = 110;
    }
    return config;
  }

  static PthBmValue make_value(uint64_t value) {
    return PthBmValue{.val = static_cast<int64_t>(value)};
  }

  static void advance_epoch(ThreadContext &ctx) {
    ++ctx.operation_count;
    if (ctx.operation_count % 50 == 0) {
      globalepoch += 2;
      active_epoch = threadinfo::min_active_epoch();
      ctx.ti->rcu_quiesce();
    }
  }

  void preload() {
    query<row_type> query;
    for (size_t i = 0; i < config_.table_size; ++i) {
      bool inserted = tree_->insert(PthBmKey(i + 1), make_value(i + 1), main_ti_,
                                    query, 0);
      assert(inserted);
    }
  }

  pth_bm_target_config config_;
  threadinfo *main_ti_;
  std::unique_ptr<PthBmTree> tree_;
  std::atomic<uint32_t> next_thread_id_{0};

  static thread_local std::unordered_map<PthBmTarget *, ThreadContext>
      g_thread_contexts_;
};

thread_local std::unordered_map<PthBmTarget *, ThreadContext>
    PthBmTarget::g_thread_contexts_;

}  // namespace

extern "C" {

pth_bm_target_config pth_bm_target_default_config(void) {
  return pth_bm_target_config{
      .table_size = 0,
      .cxl_percentage = 0,
      .max_local_memory_usage_mib = 110,
  };
}

void *pth_bm_target_create(void) {
  const pth_bm_target_config config = pth_bm_target_default_config();
  return pth_bm_target_create_with_config(&config);
}

void *pth_bm_target_create_with_config(const pth_bm_target_config *config) {
  const pth_bm_target_config effective =
      config == nullptr ? pth_bm_target_default_config() : *config;
  return new PthBmTarget(effective);
}

void pth_bm_target_init_thread(void *target) {
  auto *wrapper = static_cast<PthBmTarget *>(target);
  assert(wrapper != nullptr);
  wrapper->init_thread();
}

void pth_bm_target_print_stat(void *target) { (void)target; }

void pth_bm_target_destroy(void *target) {
  auto *wrapper = static_cast<PthBmTarget *>(target);
  delete wrapper;
}

void pth_bm_target_read(void *target, uint64_t key) {
  static_cast<PthBmTarget *>(target)->read(key);
}

void pth_bm_target_insert(void *target, uint64_t key) {
  static_cast<PthBmTarget *>(target)->insert(key);
}

void pth_bm_target_update(void *target, uint64_t key) {
  static_cast<PthBmTarget *>(target)->update(key);
}

void pth_bm_target_delete(void *target, uint64_t key) {
  static_cast<PthBmTarget *>(target)->remove(key);
}

}  // extern "C"
