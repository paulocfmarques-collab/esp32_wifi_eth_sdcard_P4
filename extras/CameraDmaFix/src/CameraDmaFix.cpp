#include "CameraDmaFix.h"
#include <atomic>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
std::atomic<TaskHandle_t> startupTask{nullptr};
std::atomic<unsigned> internalAllocations{0};
static_assert(std::atomic<TaskHandle_t>::is_always_lock_free,
              "DMA allocation guard requires a lock-free task pointer");
}

CameraDmaInternalScope::CameraDmaInternalScope() {
    internalAllocations.store(0, std::memory_order_relaxed);
    startupTask.store(xTaskGetCurrentTaskHandle(), std::memory_order_relaxed);
    // Verify the actual linked call path, not merely the presence of this class.
    void* probe = heap_caps_calloc(1, 16, MALLOC_CAP_DEFAULT);
    _ready = probe && esp_ptr_internal(probe) &&
             internalAllocations.load(std::memory_order_relaxed) != 0;
    heap_caps_free(probe);
    internalAllocations.store(0, std::memory_order_relaxed);
}

unsigned CameraDmaInternalScope::allocations() const {
    return internalAllocations.load(std::memory_order_relaxed);
}

CameraDmaInternalScope::~CameraDmaInternalScope() {
    startupTask.store(nullptr, std::memory_order_relaxed);
}

extern "C" void* __real_heap_caps_calloc(size_t count, size_t size, uint32_t caps);

extern "C" void* IRAM_ATTR __wrap_heap_caps_calloc(size_t count, size_t size, uint32_t caps) {
    // The CSI controller context uses this explicit call, bypassing the malloc
    // threshold. Its DW-GDMA callback requires an internal-RAM context.
    const TaskHandle_t owner = startupTask.load(std::memory_order_relaxed);
    if (owner && !xPortInIsrContext() && owner == xTaskGetCurrentTaskHandle() &&
        caps == MALLOC_CAP_DEFAULT) {
        caps |= MALLOC_CAP_INTERNAL;
        internalAllocations.fetch_add(1, std::memory_order_relaxed);
    }
    return __real_heap_caps_calloc(count, size, caps);
}
