/*
 * Copyright (C) 2020 The LineageOS Project
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

#define LOG_TAG "FingerprintInscreenService"

#include "FingerprintInscreen.h"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include <fstream>
#include <thread>

namespace vendor {
namespace lineage {
namespace biometrics {
namespace fingerprint {
namespace inscreen {
namespace V1_0 {
namespace implementation {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);

    if (!file) {
        PLOG(ERROR) << "Failed to open: " << path;
        return;
    }

    LOG(DEBUG) << "write: " << path << " value: " << value;

    file << value << std::endl;

    if (!file) {
        PLOG(ERROR) << "Failed to write: " << path << " value: " << value;
    }
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);

    if (!file) {
        PLOG(ERROR) << "Failed to open: " << path;
        return def;
    }

    T result;

    file >> result;

    if (file.fail()) {
        PLOG(ERROR) << "Failed to read: " << path;
        return def;
    } else {
        LOG(DEBUG) << "read: " << path << " value: " << result;
        return result;
    }
}

static hidl_vec<int8_t> stringToVec(const std::string& str) {
    auto vec = hidl_vec<int8_t>();
    vec.resize(str.size() + 1);
    for (size_t i = 0; i < str.size(); ++i) {
        vec[i] = (int8_t) str[i];
    }
    vec[str.size()] = '\0';
    return vec;
}

void FingerprintInscreen::requestResult(int, const hidl_vec<int8_t>&) { }

FingerprintInscreen::FingerprintInscreen() {
    mSehBiometricsFingerprintService = ISehBiometricsFingerprint::getService();
#ifdef FOD_SET_RECT
    set(TSP_CMD_PATH, FOD_SET_RECT);
#endif
    set(TSP_CMD_PATH, FOD_ENABLE);
    set(MASK_BRIGHTNESS_PATH, FOD_MASK);
}

Return<void> FingerprintInscreen::onStartEnroll() { return Void(); }

Return<void> FingerprintInscreen::onFinishEnroll() { return Void(); }

Return<void> FingerprintInscreen::onPress() { 
    set(FP_GREEN_CIRCLE, "1");
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(36));
        mSehBiometricsFingerprintService->sehRequest(SEM_FINGER_STATE, 
            SEM_PARAM_PRESSED, stringToVec(SEM_AOSP_FQNAME), FingerprintInscreen::requestResult);
    }).detach();
    return Void(); 
}

Return<void> FingerprintInscreen::onRelease() { 
    mSehBiometricsFingerprintService->sehRequest(SEM_FINGER_STATE, 
        SEM_PARAM_RELEASED, stringToVec(SEM_AOSP_FQNAME), FingerprintInscreen::requestResult);
    set(FP_GREEN_CIRCLE, "0");
    return Void(); 
}

Return<void> FingerprintInscreen::onShowFODView() { 
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        set(FOD_DIMMING_PATH, "1");
    }).detach();
    return Void(); 
}

Return<void> FingerprintInscreen::onHideFODView() { 
    set(FP_GREEN_CIRCLE, "0");
    set(FOD_DIMMING_PATH, "0");
    return Void(); 
}

Return<bool> FingerprintInscreen::handleAcquired(int32_t acquiredInfo, int32_t vendorCode) {
    std::lock_guard<std::mutex> _lock(mCallbackLock);

    if (mCallback == nullptr) {
        return false;
    }

    if (acquiredInfo == FINGERPRINT_ACQUIRED_VENDOR) {
        if (vendorCode == VENDORCODE_FINGER_DOWN) {
            Return<void> ret = mCallback->onFingerDown();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerDown() error: " << ret.description();
            }
            return true;
        } else if (vendorCode == VENDORCODE_FINGER_UP) {
            Return<void> ret = mCallback->onFingerUp();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerUp() error: " << ret.description();
            }
            return true;
        }
    }

    return false;
}

Return<bool> FingerprintInscreen::handleError(int32_t, int32_t) { return false; }

Return<void> FingerprintInscreen::setLongPressEnabled(bool) { return Void(); }

Return<int32_t> FingerprintInscreen::getDimAmount(int32_t) { return 0; }

Return<bool> FingerprintInscreen::shouldBoostBrightness() { return false; }

Return<void> FingerprintInscreen::setCallback(const sp<IFingerprintInscreenCallback>& callback) {
    mCallback = callback;

    return Void();
}

Return<int32_t> FingerprintInscreen::getPositionX() { return FOD_SENSOR_X; }

Return<int32_t> FingerprintInscreen::getPositionY() { return FOD_SENSOR_Y; }

Return<int32_t> FingerprintInscreen::getSize() { return FOD_SENSOR_SIZE; }

}  // namespace implementation
}  // namespace V1_0
}  // namespace inscreen
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace lineage
}  // namespace vendor
