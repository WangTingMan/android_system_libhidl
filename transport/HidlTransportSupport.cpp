/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <hidl/HidlBinderSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/Static.h>

#include <android-base/logging.h>
#include <android/hidl/manager/1.0/IServiceManager.h>

namespace android {
namespace hardware {

void configureRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    // TODO(b/32756130) this should be transport-dependent
    configureBinderRpcThreadpool(maxThreads, callerWillJoin);
}
void joinRpcThreadpool() {
    // TODO(b/32756130) this should be transport-dependent
    joinBinderRpcThreadpool();
}

int setupTransportPolling() {
    return setupBinderPolling();
}

status_t handleTransportPoll(int /*fd*/) {
    return handleBinderPoll();
}

bool setMinSchedulerPolicy(const sp<::android::hidl::base::V1_0::IBase>& service,
                           int policy, int priority) {
    if (service->isRemote()) {
        LOG(ERROR) << "Can't set scheduler policy on remote service.";
        return false;
    }

    if (policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR) {
        LOG(ERROR) << "Invalid scheduler policy " << policy;
        return false;
    }

    if (policy == SCHED_NORMAL && (priority < -20 || priority > 19)) {
        LOG(ERROR) << "Invalid priority for SCHED_NORMAL: " << priority;
        return false;
    } else if (priority < 1 || priority > 99) {
        LOG(ERROR) << "Invalid priority for real-time policy: " << priority;
        return false;
    }

    // Due to ABI considerations, IBase cannot have a destructor to clean this up.
    // So, because this API is so infrequently used, (expected to be usually only
    // one time for a process, but it can be more), we are cleaning it up here.
    // TODO(b/37794345): if ever we update the HIDL ABI for launches in an Android
    // release in the meta-version sense, we should remove this.
    std::unique_lock<std::mutex> lock = details::gServicePrioMap.lock();

    std::vector<wp<::android::hidl::base::V1_0::IBase>> toDelete;
    for (const auto& kv : details::gServicePrioMap) {
        if (kv.first.promote() == nullptr) {
            toDelete.push_back(kv.first);
        }
    }
    for (const auto& k : toDelete) {
        details::gServicePrioMap.eraseLocked(k);
    }
    details::gServicePrioMap.setLocked(service, {policy, priority});

    return true;
}

namespace details {
int32_t getPidIfSharable() {
#if LIBHIDL_TARGET_DEBUGGABLE
    return getpid();
#else
    using android::hidl::manager::V1_0::IServiceManager;
    return static_cast<int32_t>(IServiceManager::PidConstant::NO_PID);
#endif
}
}  // namespace details

}  // namespace hardware
}  // namespace android