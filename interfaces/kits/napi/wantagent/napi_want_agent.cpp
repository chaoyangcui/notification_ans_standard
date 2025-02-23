/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "napi_want_agent.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <uv.h>

#include "hilog_wrapper.h"
#include "napi_common.h"
#include "want_agent_helper.h"

namespace OHOS {

constexpr int32_t BUSINESS_ERROR_CODE_OK = 0;

TriggerCompleteCallBack::TriggerCompleteCallBack()
{}

TriggerCompleteCallBack::~TriggerCompleteCallBack()
{}

void TriggerCompleteCallBack::SetCallbackInfo(const napi_env &env, const napi_ref &ref)
{
    triggerCompleteInfo_.env = env;
    triggerCompleteInfo_.ref = ref;
}

void TriggerCompleteCallBack::SetWantAgentInstance(const std::shared_ptr<Notification::WantAgent::WantAgent> &wantAgent)
{
    triggerCompleteInfo_.wantAgent = wantAgent;
}

void TriggerCompleteCallBack::OnSendFinished(
    const AAFwk::Want &want, int resultCode, const std::string &resultData, const AAFwk::WantParams &resultExtras)
{
    HILOG_INFO("TriggerCompleteCallBack::OnSendFinished start");
    if (triggerCompleteInfo_.ref == nullptr) {
        HILOG_INFO("triggerCompleteInfo_ CallBack is nullptr");
        return;
    }
    uv_loop_s *loop = nullptr;
#if NAPI_VERSION >= 2
    napi_get_uv_event_loop(triggerCompleteInfo_.env, &loop);
#endif  // NAPI_VERSION >= 2
    uv_work_t *work = new (std::nothrow) uv_work_t;
    if (work == nullptr) {
        HILOG_INFO("uv_work_t instance is nullptr");
        return;
    }
    TriggerReceiveDataWorker *dataWorker = new (std::nothrow) TriggerReceiveDataWorker();
    if (dataWorker == nullptr) {
        HILOG_INFO("TriggerReceiveDataWorker instance is nullptr");
        return;
    }
    dataWorker->want = want;
    dataWorker->resultCode = resultCode;
    dataWorker->resultData = resultData;
    dataWorker->resultExtras = resultExtras;
    dataWorker->env = triggerCompleteInfo_.env;
    dataWorker->ref = triggerCompleteInfo_.ref;
    dataWorker->wantAgent = triggerCompleteInfo_.wantAgent;
    work->data = (void *)dataWorker;
    uv_queue_work(loop,
        work,
        [](uv_work_t *work) {},
        [](uv_work_t *work, int status) {
            TriggerReceiveDataWorker *dataWorkerData = (TriggerReceiveDataWorker *)work->data;
            if (dataWorkerData == nullptr) {
                HILOG_INFO("TriggerReceiveDataWorker instance(uv_work_t) is nullptr");
                return;
            }
            napi_value result[2] = {0};
            napi_value callback;
            napi_value undefined;
            napi_value callResult = 0;

            result[0] = GetCallbackErrorResult(dataWorkerData->env, BUSINESS_ERROR_CODE_OK);
            napi_create_object(dataWorkerData->env, &result[1]);
            // wrap wantAgent
            napi_value wantAgentClass = nullptr;
            napi_define_class(dataWorkerData->env,
                "WantAgentClass",
                NAPI_AUTO_LENGTH,
                [](napi_env env, napi_callback_info info) -> napi_value {
                    napi_value thisVar = nullptr;
                    napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr);
                    return thisVar;
                },
                nullptr,
                0,
                nullptr,
                &wantAgentClass);
            napi_value jsWantAgent = nullptr;
            napi_new_instance(dataWorkerData->env, wantAgentClass, 0, nullptr, &jsWantAgent);
            napi_wrap(dataWorkerData->env,
                jsWantAgent,
                (void *)dataWorkerData->wantAgent.get(),
                [](napi_env env, void *data, void *hint) {},
                nullptr,
                nullptr);
            napi_set_named_property(dataWorkerData->env, result[1], "wantAgent", jsWantAgent);
            //  wrap want
            napi_value jsWant;
            jsWant = WrapWant(dataWorkerData->env, dataWorkerData->want);
            napi_set_named_property(dataWorkerData->env, result[1], "want", jsWant);
            // wrap finalCode
            napi_value jsFinalCode;
            napi_create_int32(dataWorkerData->env, dataWorkerData->resultCode, &jsFinalCode);
            napi_set_named_property(dataWorkerData->env, result[1], "finalCode", jsFinalCode);
            // wrap finalData
            napi_value jsFinalData;
            napi_create_string_utf8(
                dataWorkerData->env, dataWorkerData->resultData.c_str(), NAPI_AUTO_LENGTH, &jsFinalData);
            napi_set_named_property(dataWorkerData->env, result[1], "finalData", jsFinalCode);
            // wrap extraInfo
            napi_value jsExtraInfo;
            jsExtraInfo = WrapWantParams(dataWorkerData->env, dataWorkerData->resultExtras);
            napi_set_named_property(dataWorkerData->env, result[1], "extraInfo", jsExtraInfo);

            napi_get_undefined(dataWorkerData->env, &undefined);
            napi_get_reference_value(dataWorkerData->env, dataWorkerData->ref, &callback);
            napi_call_function(dataWorkerData->env, undefined, callback, 2, &result[0], &callResult);

            delete dataWorkerData;
            dataWorkerData = nullptr;
            delete work;
            work = nullptr;
        });

    HILOG_INFO("TriggerCompleteCallBack::OnSendFinished end");
}
napi_value WantAgentInit(napi_env env, napi_value exports)
{
    HILOG_INFO("napi_moudule Init start...");
    napi_property_descriptor desc[] = {DECLARE_NAPI_FUNCTION("getBundleName", NAPI_GetBundleName),
        DECLARE_NAPI_FUNCTION("getUid", NAPI_GetUid),
        DECLARE_NAPI_FUNCTION("cancel", NAPI_Cancel),
        DECLARE_NAPI_FUNCTION("trigger", NAPI_Trigger),
        DECLARE_NAPI_FUNCTION("equal", NAPI_Equal),
        DECLARE_NAPI_FUNCTION("getWant", NAPI_GetWant),
        DECLARE_NAPI_FUNCTION("getWantAgent", NAPI_GetWantAgent)};

    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    HILOG_INFO("napi_moudule Init end...");
    return exports;
}

void SetNamedPropertyByInteger(napi_env env, napi_value dstObj, int32_t objName, const char *propName)
{
    napi_value prop = nullptr;
    if (napi_create_int32(env, objName, &prop) == napi_ok) {
        napi_set_named_property(env, dstObj, propName, prop);
    }
}

napi_value WantAgentFlagsInit(napi_env env, napi_value exports)
{
    HILOG_INFO("%{public}s, called", __func__);

    napi_value obj = nullptr;
    napi_create_object(env, &obj);

    SetNamedPropertyByInteger(env, obj, 0, "ONE_TIME_FLAG");
    SetNamedPropertyByInteger(env, obj, 1, "NO_BUILD_FLAG");
    SetNamedPropertyByInteger(env, obj, 2, "CANCEL_PRESENT_FLAG");
    SetNamedPropertyByInteger(env, obj, 3, "UPDATE_PRESENT_FLAG");
    SetNamedPropertyByInteger(env, obj, 4, "CONSTANT_FLAG");
    SetNamedPropertyByInteger(env, obj, 5, "REPLACE_ELEMENT");
    SetNamedPropertyByInteger(env, obj, 6, "REPLACE_ACTION");
    SetNamedPropertyByInteger(env, obj, 7, "REPLACE_URI");
    SetNamedPropertyByInteger(env, obj, 8, "REPLACE_ENTITIES");
    SetNamedPropertyByInteger(env, obj, 9, "REPLACE_BUNDLE");

    napi_property_descriptor exportFuncs[] = {
        DECLARE_NAPI_PROPERTY("Flags", obj),
    };

    napi_define_properties(env, exports, sizeof(exportFuncs) / sizeof(*exportFuncs), exportFuncs);
    return exports;
}

napi_value WantAgentOperationTypeInit(napi_env env, napi_value exports)
{
    HILOG_INFO("%{public}s, called", __func__);

    napi_value obj = nullptr;
    napi_create_object(env, &obj);

    SetNamedPropertyByInteger(env, obj, 0, "UNKNOWN_TYPE");
    SetNamedPropertyByInteger(env, obj, 1, "START_ABILITY");
    SetNamedPropertyByInteger(env, obj, 2, "START_ABILITIES");
    SetNamedPropertyByInteger(env, obj, 3, "START_SERVICE");
    SetNamedPropertyByInteger(env, obj, 4, "SEND_COMMON_EVENT");
    SetNamedPropertyByInteger(env, obj, 5, "START_FOREGROUND_SERVICE");

    napi_property_descriptor exportFuncs[] = {
        DECLARE_NAPI_PROPERTY("OperationType", obj),
    };

    napi_define_properties(env, exports, sizeof(exportFuncs) / sizeof(*exportFuncs), exportFuncs);
    return exports;
}

napi_value NAPI_GetBundleNameWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncGetBundleNameCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_GetBundleNameWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetBundleNameCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetBundleName called(CallBack Mode)...");
                AsyncGetBundleNameCallbackInfo *asyncCallbackInfo = (AsyncGetBundleNameCallbackInfo *)data;
                asyncCallbackInfo->bundleName = WantAgentHelper::GetBundleName(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetBundleName compeleted(CallBack Mode)...");
                AsyncGetBundleNameCallbackInfo *asyncCallbackInfo = (AsyncGetBundleNameCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);
                napi_create_string_utf8(env, asyncCallbackInfo->bundleName.c_str(), NAPI_AUTO_LENGTH, &result[1]);
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);

                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetBundleNamePromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetBundleName called(Promise Mode)...");
                AsyncGetBundleNameCallbackInfo *asyncCallbackInfo = (AsyncGetBundleNameCallbackInfo *)data;
                asyncCallbackInfo->bundleName = WantAgentHelper::GetBundleName(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetBundleName compeleted(Promise Mode)...");
                AsyncGetBundleNameCallbackInfo *asyncCallbackInfo = (AsyncGetBundleNameCallbackInfo *)data;
                napi_value result;
                napi_create_string_utf8(env, asyncCallbackInfo->bundleName.c_str(), NAPI_AUTO_LENGTH, &result);
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_GetBundleName(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentType;
    napi_typeof(env, argv[0], &wantAgentType);
    NAPI_ASSERT(env, wantAgentType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgent = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgent));
    if (pWantAgent == nullptr) {
        HILOG_INFO("Notification::WantAgent::WantAgent napi_unwrap error");
        return NapiGetNull(env);
    }

    bool callBackMode = false;
    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[1], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncGetBundleNameCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncGetBundleNameCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgent = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgent);

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_GetBundleNameWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

napi_value NAPI_GetUidWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncGetUidCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_GetUidWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetUidCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetUid called(CallBack Mode)...");
                AsyncGetUidCallbackInfo *asyncCallbackInfo = (AsyncGetUidCallbackInfo *)data;
                asyncCallbackInfo->uid = WantAgentHelper::GetUid(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetUid compeleted(CallBack Mode)...");
                AsyncGetUidCallbackInfo *asyncCallbackInfo = (AsyncGetUidCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);
                napi_create_int32(env, asyncCallbackInfo->uid, &result[1]);
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);

                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetUidPromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetUid called(Promise Mode)...");
                AsyncGetUidCallbackInfo *asyncCallbackInfo = (AsyncGetUidCallbackInfo *)data;
                asyncCallbackInfo->uid = WantAgentHelper::GetUid(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetUid compeleted(Promise Mode)...");
                AsyncGetUidCallbackInfo *asyncCallbackInfo = (AsyncGetUidCallbackInfo *)data;
                napi_value result;
                napi_create_int32(env, asyncCallbackInfo->uid, &result);
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_GetUid(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentType;
    napi_typeof(env, argv[0], &wantAgentType);
    NAPI_ASSERT(env, wantAgentType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgent = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgent));
    if (pWantAgent == nullptr) {
        return NapiGetNull(env);
    }

    bool callBackMode = false;
    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[1], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncGetUidCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncGetUidCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgent = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgent);

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_GetUidWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

napi_value NAPI_GetWantWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncGetWantCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_GetWantWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetWantCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetWant called(CallBack Mode)...");
                AsyncGetWantCallbackInfo *asyncCallbackInfo = (AsyncGetWantCallbackInfo *)data;
                asyncCallbackInfo->want = WantAgentHelper::GetWant(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetWant compeleted(CallBack Mode)...");
                AsyncGetWantCallbackInfo *asyncCallbackInfo = (AsyncGetWantCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);
                result[1] = WrapWant(env, *(asyncCallbackInfo->want));
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);
                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetWantPromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetWant called(Promise Mode)...");
                AsyncGetWantCallbackInfo *asyncCallbackInfo = (AsyncGetWantCallbackInfo *)data;
                asyncCallbackInfo->want = WantAgentHelper::GetWant(asyncCallbackInfo->wantAgent);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetWant compeleted(Promise Mode)...");
                AsyncGetWantCallbackInfo *asyncCallbackInfo = (AsyncGetWantCallbackInfo *)data;
                napi_value result;
                result = WrapWant(env, *(asyncCallbackInfo->want));
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_GetWant(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentType;
    napi_typeof(env, argv[0], &wantAgentType);
    NAPI_ASSERT(env, wantAgentType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgent = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgent));
    if (pWantAgent == nullptr) {
        return NapiGetNull(env);
    }

    bool callBackMode = false;
    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[1], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncGetWantCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncGetWantCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgent = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgent);

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_GetWantWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

void DeleteRecordByCode(const int32_t code)
{
    std::lock_guard<std::recursive_mutex> guard(g_mutex);
    for (const auto &item : g_WantAgentMap) {
        auto code_ = item.second;
        auto record = item.first;
        if (code_ == code) {
            g_WantAgentMap.erase(record);
            if (record != nullptr) {
                delete record;
                record = nullptr;
            }
        }
    }
}

napi_value NAPI_CancelWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncCancelCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_CancelWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_CancelCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("Cancel called(CallBack Mode)...");
                AsyncCancelCallbackInfo *asyncCallbackInfo = (AsyncCancelCallbackInfo *)data;
                WantAgentHelper::Cancel(asyncCallbackInfo->wantAgent);
                int32_t code = WantAgentHelper::GetHashCode(asyncCallbackInfo->wantAgent);
                DeleteRecordByCode(code);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("Cancel compeleted(CallBack Mode)...");
                AsyncCancelCallbackInfo *asyncCallbackInfo = (AsyncCancelCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);
                napi_get_null(env, &result[1]);
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);

                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_CancelPromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("Cancel called(Promise Mode)...");
                AsyncCancelCallbackInfo *asyncCallbackInfo = (AsyncCancelCallbackInfo *)data;
                WantAgentHelper::Cancel(asyncCallbackInfo->wantAgent);
                int32_t code = WantAgentHelper::GetHashCode(asyncCallbackInfo->wantAgent);
                DeleteRecordByCode(code);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("Cancel compeleted(Promise Mode)...");
                AsyncCancelCallbackInfo *asyncCallbackInfo = (AsyncCancelCallbackInfo *)data;
                napi_value result;
                napi_get_null(env, &result);
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_Cancel(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentType;
    napi_typeof(env, argv[0], &wantAgentType);
    NAPI_ASSERT(env, wantAgentType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgent = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgent));
    if (pWantAgent == nullptr) {
        return NapiGetNull(env);
    }

    bool callBackMode = false;
    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[1], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncCancelCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncCancelCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgent = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgent);

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_CancelWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

napi_value NAPI_TriggerWrap(napi_env env, napi_callback_info info, AsyncTriggerCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_TriggerWrap called...");
    napi_value resourceName;
    napi_create_string_latin1(env, "NAPI_TriggerWrap", NAPI_AUTO_LENGTH, &resourceName);

    napi_create_async_work(env,
        nullptr,
        resourceName,
        [](napi_env env, void *data) {
            HILOG_INFO("Trigger called ...");
            AsyncTriggerCallbackInfo *asyncCallbackInfo = (AsyncTriggerCallbackInfo *)data;
            asyncCallbackInfo->triggerObj->SetCallbackInfo(env, asyncCallbackInfo->callback[0]);
            asyncCallbackInfo->triggerObj->SetWantAgentInstance(asyncCallbackInfo->wantAgent);
            WantAgentHelper::TriggerWantAgent(asyncCallbackInfo->context,
                asyncCallbackInfo->wantAgent,
                asyncCallbackInfo->triggerObj,
                asyncCallbackInfo->triggerInfo);
        },
        [](napi_env env, napi_status status, void *data) {
            HILOG_INFO("Trigger compeleted ...");
            AsyncTriggerCallbackInfo *asyncCallbackInfo = (AsyncTriggerCallbackInfo *)data;
            napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
            delete asyncCallbackInfo;
        },
        (void *)asyncCallbackInfo,
        &asyncCallbackInfo->asyncWork);

    NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
    // create reutrn
    napi_value ret = 0;
    NAPI_CALL(env, napi_create_int32(env, 0, &ret));
    return ret;
}

napi_value NAPI_Trigger(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentType;
    napi_typeof(env, argv[0], &wantAgentType);
    NAPI_ASSERT(env, wantAgentType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgent = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgent));
    if (pWantAgent == nullptr) {
        return NapiGetNull(env);
    }

    // Get triggerInfo
    napi_value jsTriggerInfo = argv[1];
    napi_valuetype valueType;
    NAPI_CALL(env, napi_typeof(env, jsTriggerInfo, &valueType));
    NAPI_ASSERT(env, valueType == napi_object, "param type mismatch!");

    // Get triggerInfo code
    int32_t code = -1;
    if (!UnwrapInt32ByPropertyName(env, jsTriggerInfo, "code", code)) {
        return NapiGetNull(env);
    }
    // Get triggerInfo want
    napi_value jsWant = nullptr;
    jsWant = GetPropertyValueByPropertyName(env, jsTriggerInfo, "want", napi_object);
    std::shared_ptr<AAFwk::Want> want = nullptr;
    if (jsWant != nullptr) {
        want = std::make_shared<AAFwk::Want>();
        if (!UnwrapWant(env, jsWant, *want)) {
            return NapiGetNull(env);
        }
    }
    // Get triggerInfo permission
    std::string permission;
    UnwrapStringByPropertyName(env, jsTriggerInfo, "permission", permission);
    // Get triggerInfo extraInfo
    napi_value jsExtraInfo = nullptr;
    jsExtraInfo = GetPropertyValueByPropertyName(env, jsTriggerInfo, "extraInfo", napi_object);
    std::shared_ptr<AAFwk::WantParams> extraInfo = nullptr;
    if (jsExtraInfo != nullptr) {
        extraInfo = std::make_shared<AAFwk::WantParams>();
        if (!UnwrapWantParams(env, jsExtraInfo, *extraInfo)) {
            return NapiGetNull(env);
        }
    }
    // Get context
    napi_value global = 0;
    NAPI_CALL(env, napi_get_global(env, &global));
    napi_value abilityObj = 0;
    NAPI_CALL(env, napi_get_named_property(env, global, "ability", &abilityObj));
    Ability *ability = nullptr;
    NAPI_CALL(env, napi_get_value_external(env, abilityObj, (void **)&ability));
    std::shared_ptr<OHOS::AppExecFwk::Context> context = nullptr;
    context = ability->GetContext();

    bool callBackMode = false;
    if (argc >= 3) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[2], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncTriggerCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncTriggerCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgent = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgent);
    asyncCallbackInfo->context = context;
    Notification::WantAgent::TriggerInfo triggerInfo(permission, extraInfo, want, code);
    asyncCallbackInfo->triggerInfo = triggerInfo;
    asyncCallbackInfo->triggerObj = nullptr;
    if (callBackMode) {
        asyncCallbackInfo->callBackMode = callBackMode;
        asyncCallbackInfo->triggerObj = std::make_shared<TriggerCompleteCallBack>();
        napi_create_reference(env, argv[2], 1, &asyncCallbackInfo->callback[0]);
    }

    napi_value ret = NAPI_TriggerWrap(env, info, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return NapiGetNull(env);
}

napi_value NAPI_EqualWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncEqualCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_EqualWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_EqualWrapCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("Equal called(CallBack Mode)...");
                AsyncEqualCallbackInfo *asyncCallbackInfo = (AsyncEqualCallbackInfo *)data;
                asyncCallbackInfo->result = WantAgentHelper::JudgeEquality(
                    asyncCallbackInfo->wantAgentFirst, asyncCallbackInfo->wantAgentSecond);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("Equal compeleted(CallBack Mode)...");
                AsyncEqualCallbackInfo *asyncCallbackInfo = (AsyncEqualCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);
                napi_get_boolean(env, asyncCallbackInfo->result, &result[1]);
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);

                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_EqualPromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("Equal called(Promise Mode)...");
                AsyncEqualCallbackInfo *asyncCallbackInfo = (AsyncEqualCallbackInfo *)data;
                asyncCallbackInfo->result = WantAgentHelper::JudgeEquality(
                    asyncCallbackInfo->wantAgentFirst, asyncCallbackInfo->wantAgentSecond);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("Equal compeleted(Promise Mode)...");
                AsyncEqualCallbackInfo *asyncCallbackInfo = (AsyncEqualCallbackInfo *)data;
                napi_value result;
                napi_get_boolean(env, asyncCallbackInfo->result, &result);
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
                delete asyncCallbackInfo;
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_Equal(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype wantAgentFirstType;
    napi_typeof(env, argv[0], &wantAgentFirstType);
    NAPI_ASSERT(env, wantAgentFirstType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgentFirst = nullptr;
    napi_unwrap(env, argv[0], (void **)&(pWantAgentFirst));
    if (pWantAgentFirst == nullptr) {
        return NapiGetNull(env);
    }

    napi_valuetype wantAgentSecondType;
    napi_typeof(env, argv[1], &wantAgentSecondType);
    NAPI_ASSERT(env, wantAgentSecondType == napi_object, "Wrong argument type. Object expected.");

    Notification::WantAgent::WantAgent *pWantAgentSecond = nullptr;
    napi_unwrap(env, argv[1], (void **)&(pWantAgentSecond));
    if (pWantAgentSecond == nullptr) {
        return NapiGetNull(env);
    }

    bool callBackMode = false;
    if (argc >= 3) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[2], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }
    AsyncEqualCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncEqualCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wantAgentFirst = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgentFirst);
    asyncCallbackInfo->wantAgentSecond = std::make_shared<Notification::WantAgent::WantAgent>(*pWantAgentSecond);

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_EqualWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }
    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

napi_value NAPI_GetWantAgentWrap(
    napi_env env, napi_callback_info info, bool callBackMode, AsyncGetWantAgentCallbackInfo *asyncCallbackInfo)
{
    HILOG_INFO("NAPI_GetWantAgentWrap called...");
    if (callBackMode) {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetWantAgentCallBack", NAPI_AUTO_LENGTH, &resourceName);

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetWantAgent called(CallBack Mode)...");
                AsyncGetWantAgentCallbackInfo *asyncCallbackInfo = (AsyncGetWantAgentCallbackInfo *)data;
                Notification::WantAgent::WantAgentInfo wantAgentInfo(asyncCallbackInfo->requestCode,
                    asyncCallbackInfo->operationType,
                    asyncCallbackInfo->wantAgentFlags,
                    asyncCallbackInfo->wants,
                    asyncCallbackInfo->extraInfo);
                asyncCallbackInfo->wantAgent =
                    Notification::WantAgent::WantAgentHelper::GetWantAgent(asyncCallbackInfo->context, wantAgentInfo);
                if (asyncCallbackInfo->wantAgent == nullptr) {
                    HILOG_INFO("GetWantAgent instance is nullptr...");
                }
                int32_t code = Notification::WantAgent::WantAgentHelper::GetHashCode(asyncCallbackInfo->wantAgent);
                std::lock_guard<std::recursive_mutex> guard(g_mutex);
                g_WantAgentMap.emplace(asyncCallbackInfo, code);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetWantAgent compeleted(CallBack Mode)...");
                AsyncGetWantAgentCallbackInfo *asyncCallbackInfo = (AsyncGetWantAgentCallbackInfo *)data;
                napi_value result[2] = {0};
                napi_value callback;
                napi_value undefined;
                napi_value callResult = 0;

                result[0] = GetCallbackErrorResult(asyncCallbackInfo->env, BUSINESS_ERROR_CODE_OK);

                napi_value wantAgentClass = nullptr;
                napi_define_class(env,
                    "WantAgentClass",
                    NAPI_AUTO_LENGTH,
                    [](napi_env env, napi_callback_info info) -> napi_value {
                        napi_value thisVar = nullptr;
                        napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr);
                        return thisVar;
                    },
                    nullptr,
                    0,
                    nullptr,
                    &wantAgentClass);
                napi_new_instance(env, wantAgentClass, 0, nullptr, &result[1]);
                napi_wrap(env,
                    result[1],
                    (void *)asyncCallbackInfo->wantAgent.get(),
                    [](napi_env env, void *data, void *hint) {},
                    nullptr,
                    nullptr);
                napi_get_undefined(env, &undefined);
                napi_get_reference_value(env, asyncCallbackInfo->callback[0], &callback);
                napi_call_function(env, undefined, callback, 2, &result[0], &callResult);

                if (asyncCallbackInfo->callback[0] != nullptr) {
                    napi_delete_reference(env, asyncCallbackInfo->callback[0]);
                }
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);

        NAPI_CALL(env, napi_queue_async_work(env, asyncCallbackInfo->asyncWork));
        // create reutrn
        napi_value ret = 0;
        NAPI_CALL(env, napi_create_int32(env, 0, &ret));
        return ret;
    } else {
        napi_value resourceName;
        napi_create_string_latin1(env, "NAPI_GetWantAgentPromise", NAPI_AUTO_LENGTH, &resourceName);

        napi_deferred deferred;
        napi_value promise;
        NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));
        asyncCallbackInfo->deferred = deferred;

        napi_create_async_work(env,
            nullptr,
            resourceName,
            [](napi_env env, void *data) {
                HILOG_INFO("GetWantAgent called(Promise Mode)...");
                AsyncGetWantAgentCallbackInfo *asyncCallbackInfo = (AsyncGetWantAgentCallbackInfo *)data;
                HILOG_INFO("GetWantAgent wants.size = [%{public}zu], wantAgentFlags.size = [%{public}zu]",
                    asyncCallbackInfo->wants.size(),
                    asyncCallbackInfo->wantAgentFlags.size());

                Notification::WantAgent::WantAgentInfo wantAgentInfo(asyncCallbackInfo->requestCode,
                    asyncCallbackInfo->operationType,
                    asyncCallbackInfo->wantAgentFlags,
                    asyncCallbackInfo->wants,
                    asyncCallbackInfo->extraInfo);
                asyncCallbackInfo->wantAgent =
                    Notification::WantAgent::WantAgentHelper::GetWantAgent(asyncCallbackInfo->context, wantAgentInfo);
                if (asyncCallbackInfo->wantAgent == nullptr) {
                    HILOG_INFO("GetWantAgent instance is nullptr...");
                }
                int32_t code = Notification::WantAgent::WantAgentHelper::GetHashCode(asyncCallbackInfo->wantAgent);
                std::lock_guard<std::recursive_mutex> guard(g_mutex);
                g_WantAgentMap.emplace(asyncCallbackInfo, code);
            },
            [](napi_env env, napi_status status, void *data) {
                HILOG_INFO("GetWantAgent compeleted(Promise Mode)...");
                AsyncGetWantAgentCallbackInfo *asyncCallbackInfo = (AsyncGetWantAgentCallbackInfo *)data;
                napi_value wantAgentClass = nullptr;
                napi_define_class(env,
                    "WantAgentClass",
                    NAPI_AUTO_LENGTH,
                    [](napi_env env, napi_callback_info info) -> napi_value {
                        napi_value thisVar = nullptr;
                        napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr);
                        return thisVar;
                    },
                    nullptr,
                    0,
                    nullptr,
                    &wantAgentClass);
                napi_value result = nullptr;
                napi_new_instance(env, wantAgentClass, 0, nullptr, &result);
                napi_wrap(env,
                    result,
                    (void *)asyncCallbackInfo->wantAgent.get(),
                    [](napi_env env, void *data, void *hint) {},
                    nullptr,
                    nullptr);
                napi_resolve_deferred(asyncCallbackInfo->env, asyncCallbackInfo->deferred, result);
                napi_delete_async_work(env, asyncCallbackInfo->asyncWork);
            },
            (void *)asyncCallbackInfo,
            &asyncCallbackInfo->asyncWork);
        napi_queue_async_work(env, asyncCallbackInfo->asyncWork);
        return promise;
    }
}

napi_value NAPI_GetWantAgent(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[argc];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    HILOG_INFO("argc = [%{public}zu]", argc);

    napi_valuetype jsWantAgentInfoType;
    napi_value jsWantAgentInfo = argv[0];
    NAPI_CALL(env, napi_typeof(env, jsWantAgentInfo, &jsWantAgentInfoType));
    NAPI_ASSERT(env, jsWantAgentInfoType == napi_object, "param type mismatch!");

    napi_value jsWants = nullptr;
    jsWants = GetPropertyValueByPropertyName(env, jsWantAgentInfo, "wants", napi_object);
    if (jsWants == nullptr) {
        return NapiGetNull(env);
    }
    bool isArray = false;
    if (napi_is_array(env, jsWants, &isArray) != napi_ok) {
        return NapiGetNull(env);
    }
    if (isArray == false) {
        return NapiGetNull(env);
    }
    uint32_t wantsLen = 0;
    napi_get_array_length(env, jsWants, &wantsLen);
    if (wantsLen < 0) {
        return NapiGetNull(env);
    }
    std::vector<std::shared_ptr<AAFwk::Want>> wants;
    for (uint32_t i = 0; i < wantsLen; i++) {
        std::shared_ptr<AAFwk::Want> want = std::make_shared<AAFwk::Want>();
        napi_value jsWant;
        napi_get_element(env, jsWants, i, &jsWant);
        if (!UnwrapWant(env, jsWant, *want)) {
            return NapiGetNull(env);
        }
        HILOG_INFO("want type is [%{public}s]", want->GetType().c_str());
        wants.emplace_back(want);
    }
    // Get operationType
    int32_t operationType = -1;
    if (!UnwrapInt32ByPropertyName(env, jsWantAgentInfo, "operationType", operationType)) {
        return NapiGetNull(env);
    }
    // Get requestCode
    int32_t requestCode = -1;
    if (!UnwrapInt32ByPropertyName(env, jsWantAgentInfo, "requestCode", requestCode)) {
        return NapiGetNull(env);
    }
    // Get wantAgentFlags
    napi_value JsWantAgentFlags = nullptr;
    std::vector<Notification::WantAgent::WantAgentConstant::Flags> wantAgentFlags;
    JsWantAgentFlags = GetPropertyValueByPropertyName(env, jsWantAgentInfo, "wantAgentFlags", napi_object);
    HILOG_INFO("NAPI_GetWantAgent8");
    if (JsWantAgentFlags != nullptr) {
        uint32_t arrayLength = 0;
        NAPI_CALL(env, napi_get_array_length(env, JsWantAgentFlags, &arrayLength));
        HILOG_INFO("property is array, length=%{public}d", arrayLength);
        for (uint32_t i = 0; i < arrayLength; i++) {
            napi_value napiWantAgentFlags;
            napi_get_element(env, JsWantAgentFlags, i, &napiWantAgentFlags);
            napi_valuetype valuetype0;
            NAPI_CALL(env, napi_typeof(env, napiWantAgentFlags, &valuetype0));
            NAPI_ASSERT(env, valuetype0 == napi_number, "Wrong argument type. Numbers expected.");
            int32_t value0 = 0;
            NAPI_CALL(env, napi_get_value_int32(env, napiWantAgentFlags, &value0));
            wantAgentFlags.emplace_back(static_cast<Notification::WantAgent::WantAgentConstant::Flags>(value0));
        }
    }
    // Get extraInfo
    napi_value JsExtraInfo = nullptr;
    JsExtraInfo = GetPropertyValueByPropertyName(env, jsWantAgentInfo, "extraInfo", napi_object);
    AAFwk::WantParams extraInfo;
    if (JsExtraInfo != nullptr) {
        if (!UnwrapWantParams(env, JsExtraInfo, extraInfo)) {
            return NapiGetNull(env);
        }
    }
    // Get context
    napi_value global = 0;
    NAPI_CALL(env, napi_get_global(env, &global));
    napi_value abilityObj = 0;
    NAPI_CALL(env, napi_get_named_property(env, global, "ability", &abilityObj));
    Ability *ability = nullptr;
    NAPI_CALL(env, napi_get_value_external(env, abilityObj, (void **)&ability));
    std::shared_ptr<OHOS::AppExecFwk::Context> context = nullptr;
    context = ability->GetContext();

    bool callBackMode = false;
    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, argv[1], &valuetype));
        NAPI_ASSERT(env, valuetype == napi_function, "Wrong argument type. Function expected.");
        callBackMode = true;
    }

    AsyncGetWantAgentCallbackInfo *asyncCallbackInfo =
        new (std::nothrow) AsyncGetWantAgentCallbackInfo{.env = env, .asyncWork = nullptr, .deferred = nullptr};
    if (asyncCallbackInfo == nullptr) {
        return NapiGetNull(env);
    }
    asyncCallbackInfo->wants = wants;
    asyncCallbackInfo->operationType =
        static_cast<Notification::WantAgent::WantAgentConstant::OperationType>(operationType);
    asyncCallbackInfo->requestCode = requestCode;
    asyncCallbackInfo->wantAgentFlags = wantAgentFlags;
    asyncCallbackInfo->extraInfo.reset(new (std::nothrow) AAFwk::WantParams(extraInfo));
    asyncCallbackInfo->context = context;

    if (callBackMode) {
        napi_create_reference(env, argv[1], 1, &asyncCallbackInfo->callback[0]);
    }
    napi_value ret = NAPI_GetWantAgentWrap(env, info, callBackMode, asyncCallbackInfo);
    if (ret == nullptr) {
        delete asyncCallbackInfo;
        asyncCallbackInfo = nullptr;
    }

    return ((callBackMode) ? (NapiGetNull(env)) : (ret));
}

napi_value GetCallbackErrorResult(napi_env env, int errCode)
{
    napi_value result = nullptr;
    napi_value eCode = nullptr;
    NAPI_CALL(env, napi_create_int32(env, errCode, &eCode));
    NAPI_CALL(env, napi_create_object(env, &result));
    NAPI_CALL(env, napi_set_named_property(env, result, "code", eCode));
    return result;
}

napi_value NapiGetNull(napi_env env)
{
    napi_value result = 0;
    napi_get_null(env, &result);
    return result;
}

}  // namespace OHOS