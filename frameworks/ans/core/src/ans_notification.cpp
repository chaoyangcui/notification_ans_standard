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

#include "ans_notification.h"
#include "ans_inner_errors.h"
#include "ans_log_wrapper.h"
#include "iservice_registry.h"
#include "notification_request.h"
#include "notification_sorting.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace Notification {
ErrCode AnsNotification::AddNotificationSlot(const NotificationSlot &slot)
{
    std::vector<NotificationSlot> slots;
    slots.push_back(slot);
    return AddNotificationSlots(slots);
}

ErrCode AnsNotification::AddNotificationSlots(const std::vector<NotificationSlot> &slots)
{
    if (slots.size() == 0) {
        ANS_LOGE("Failed to add notification slots because input slots size is 0.");
        return ERR_ANS_INVALID_PARAM;
    }
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    std::vector<sptr<NotificationSlot>> slotsSptr;
    for (auto it = slots.begin(); it != slots.end(); ++it) {
        sptr<NotificationSlot> slot = new NotificationSlot(*it);
        if (slot == nullptr) {
            ANS_LOGE("Failed to create NotificationSlot ptr.");
            return ERR_ANS_NO_MEMORY;
        }
        slotsSptr.emplace_back(slot);
    }

    return ansManagerProxy_->AddSlots(slotsSptr);
}

ErrCode AnsNotification::RemoveNotificationSlot(const NotificationConstant::SlotType &slotType)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->RemoveSlotByType(slotType);
}

ErrCode AnsNotification::GetNotificationSlot(
    const NotificationConstant::SlotType &slotType, sptr<NotificationSlot> &slot)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSlotByType(slotType, slot);
}

ErrCode AnsNotification::GetNotificationSlots(std::vector<sptr<NotificationSlot>> &slots)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSlots(slots);
}

ErrCode AnsNotification::AddNotificationSlotGroup(const NotificationSlotGroup &slotGroup)
{
    std::vector<NotificationSlotGroup> slotGroups;
    slotGroups.emplace_back(slotGroup);
    return AddNotificationSlotGroups(slotGroups);
}

ErrCode AnsNotification::AddNotificationSlotGroups(const std::vector<NotificationSlotGroup> &slotGroups)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    std::vector<sptr<NotificationSlotGroup>> slotGroupsSptr;
    for (auto it = slotGroups.begin(); it != slotGroups.end(); ++it) {
        sptr<NotificationSlotGroup> slotGroup = new NotificationSlotGroup((*it).GetId(), (*it).GetName());
        if (slotGroup == nullptr) {
            ANS_LOGE("Failed to add notification slot groups with NotificationSlotGroup nullptr");
            return ERR_ANS_NO_MEMORY;
        }
        slotGroupsSptr.emplace_back(slotGroup);
    }

    return ansManagerProxy_->AddSlotGroups(slotGroupsSptr);
}

ErrCode AnsNotification::RemoveNotificationSlotGroup(const std::string &slotGroupId)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    std::vector<std::string> slotGroupIds;
    slotGroupIds.emplace_back(slotGroupId);
    return ansManagerProxy_->RemoveSlotGroups(slotGroupIds);
}

ErrCode AnsNotification::GetNotificationSlotGroup(const std::string &groupId, sptr<NotificationSlotGroup> &group)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSlotGroup(groupId, group);
}

ErrCode AnsNotification::GetNotificationSlotGroups(std::vector<sptr<NotificationSlotGroup>> &groups)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSlotGroups(groups);
}

ErrCode AnsNotification::PublishNotification(const NotificationRequest &request)
{
    ANS_LOGI("enter");
    return PublishNotification(std::string(), request);
}

ErrCode AnsNotification::PublishNotification(const std::string &label, const NotificationRequest &request)
{
    ANS_LOGI("enter");

    if (request.GetContent() == nullptr || request.GetNotificationType() == NotificationContent::Type::NONE) {
        ANS_LOGE("Refuse to publish the notification without valid content");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!CanPublishMediaContent(request)) {
        ANS_LOGE("Refuse to publish the notification because the sequence numbers actions not match those assigned to "
                 "added action buttons.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationRequest> reqPtr = new (std::nothrow) NotificationRequest(request);
    if (reqPtr == nullptr) {
        ANS_LOGE("Failed to create NotificationRequest ptr");
        return ERR_ANS_NO_MEMORY;
    }
    return ansManagerProxy_->Publish(label, reqPtr);
}

ErrCode AnsNotification::PublishNotification(const NotificationRequest &request, const std::string &deviceId)
{
    if (request.GetContent() == nullptr || request.GetNotificationType() == NotificationContent::Type::NONE) {
        ANS_LOGE("Refuse to publish the notification without valid content");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!CanPublishMediaContent(request)) {
        ANS_LOGE("Refuse to publish the notification because the sequence numbers actions not match those assigned to "
                 "added action buttons.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationRequest> reqPtr = new (std::nothrow) NotificationRequest(request);
    if (reqPtr == nullptr) {
        ANS_LOGE("Failed to create NotificationRequest ptr");
        return ERR_ANS_NO_MEMORY;
    }
    return ansManagerProxy_->PublishToDevice(reqPtr, deviceId);
}

ErrCode AnsNotification::CancelNotification(int32_t notificationId)
{
    return CancelNotification("", notificationId);
}

ErrCode AnsNotification::CancelNotification(const std::string &label, int32_t notificationId)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->Cancel(notificationId, label);
}

ErrCode AnsNotification::CancelAllNotifications()
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->CancelAll();
}

ErrCode AnsNotification::GetActiveNotificationNums(int32_t &num)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetActiveNotificationNums(num);
}

ErrCode AnsNotification::GetActiveNotifications(std::vector<sptr<NotificationRequest>> &request)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetActiveNotifications(request);
}

ErrCode AnsNotification::GetCurrentAppSorting(sptr<NotificationSortingMap> &sortingMap)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetCurrentAppSorting(sortingMap);
}

ErrCode AnsNotification::SetNotificationAgent(const std::string &agent)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetNotificationAgent(agent);
}

ErrCode AnsNotification::GetNotificationAgent(std::string &agent)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetNotificationAgent(agent);
}

ErrCode AnsNotification::CanPublishNotificationAsBundle(const std::string &representativeBundle, bool &canPublish)
{
    if (representativeBundle.empty()) {
        ANS_LOGW("Input representativeBundle is empty");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->CanPublishAsBundle(representativeBundle, canPublish);
}

ErrCode AnsNotification::PublishNotificationAsBundle(
    const std::string &representativeBundle, const NotificationRequest &request)
{
    if (representativeBundle.empty()) {
        ANS_LOGE("Refuse to publish the notification whit invalid representativeBundle");
        return ERR_ANS_INVALID_PARAM;
    }

    if (request.GetContent() == nullptr || request.GetNotificationType() == NotificationContent::Type::NONE) {
        ANS_LOGE("Refuse to publish the notification without valid content");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!CanPublishMediaContent(request)) {
        ANS_LOGE("Refuse to publish the notification because the sequence numbers actions not match those assigned to "
                 "added action buttons.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationRequest> reqPtr = new (std::nothrow) NotificationRequest(request);
    if (reqPtr == nullptr) {
        ANS_LOGE("Failed to create NotificationRequest ptr");
        return ERR_ANS_NO_MEMORY;
    }
    return ansManagerProxy_->PublishAsBundle(reqPtr, representativeBundle);
}

ErrCode AnsNotification::SetNotificationBadgeNum()
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    int32_t num = -1;
    return ansManagerProxy_->SetNotificationBadgeNum(num);
}

ErrCode AnsNotification::SetNotificationBadgeNum(int32_t num)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetNotificationBadgeNum(num);
}

ErrCode AnsNotification::IsAllowedNotify(bool &allowed)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->IsAllowedNotify(allowed);
}

ErrCode AnsNotification::AreNotificationsSuspended(bool &suspended)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->AreNotificationsSuspended(suspended);
}

ErrCode AnsNotification::HasNotificationPolicyAccessPermission(bool &hasPermission)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->HasNotificationPolicyAccessPermission(hasPermission);
}

ErrCode AnsNotification::GetBundleImportance(NotificationSlot::NotificationLevel &importance)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    int importanceTemp;
    ErrCode ret = ansManagerProxy_->GetBundleImportance(importanceTemp);
    if ((NotificationSlot::LEVEL_NONE <= importanceTemp) && (importanceTemp <= NotificationSlot::LEVEL_HIGH)) {
        importance = static_cast<NotificationSlot::NotificationLevel>(importanceTemp);
    } else {
        importance = NotificationSlot::LEVEL_UNDEFINED;
    }
    return ret;
}

ErrCode AnsNotification::SubscribeNotification(const NotificationSubscriber &subscriber)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationSubscriber::SubscriberImpl> subscriberSptr = subscriber.GetImpl();
    if (subscriberSptr == nullptr) {
        ANS_LOGE("Failed to subscribe with SubscriberImpl null ptr.");
        return ERR_ANS_INVALID_PARAM;
    }
    return ansManagerProxy_->Subscribe(subscriberSptr, nullptr);
}

ErrCode AnsNotification::SubscribeNotification(
    const NotificationSubscriber &subscriber, const NotificationSubscribeInfo &subscribeInfo)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationSubscribeInfo> sptrInfo = new (std::nothrow) NotificationSubscribeInfo(subscribeInfo);
    if (sptrInfo == nullptr) {
        ANS_LOGE("Failed to create NotificationSubscribeInfo ptr.");
        return ERR_ANS_NO_MEMORY;
    }

    sptr<NotificationSubscriber::SubscriberImpl> subscriberSptr = subscriber.GetImpl();
    if (subscriberSptr == nullptr) {
        ANS_LOGE("Failed to subscribe with SubscriberImpl null ptr.");
        return ERR_ANS_INVALID_PARAM;
    }
    return ansManagerProxy_->Subscribe(subscriberSptr, sptrInfo);
}

ErrCode AnsNotification::UnSubscribeNotification(NotificationSubscriber &subscriber)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationSubscriber::SubscriberImpl> subscriberSptr = subscriber.GetImpl();
    if (subscriberSptr == nullptr) {
        ANS_LOGE("Failed to unsubscribe with SubscriberImpl null ptr.");
        return ERR_ANS_INVALID_PARAM;
    }
    return ansManagerProxy_->Unsubscribe(subscriberSptr, nullptr);
}

ErrCode AnsNotification::UnSubscribeNotification(
    NotificationSubscriber &subscriber, NotificationSubscribeInfo subscribeInfo)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }

    sptr<NotificationSubscribeInfo> sptrInfo = new (std::nothrow) NotificationSubscribeInfo(subscribeInfo);
    if (sptrInfo == nullptr) {
        ANS_LOGE("Failed to create NotificationSubscribeInfo ptr.");
        return ERR_ANS_NO_MEMORY;
    }

    sptr<NotificationSubscriber::SubscriberImpl> subscriberSptr = subscriber.GetImpl();
    if (subscriberSptr == nullptr) {
        ANS_LOGE("Failed to unsubscribe with SubscriberImpl null ptr.");
        return ERR_ANS_INVALID_PARAM;
    }
    return ansManagerProxy_->Unsubscribe(subscriberSptr, sptrInfo);
}

ErrCode AnsNotification::RemoveNotification(const std::string &key)
{
    if (key.empty()) {
        ANS_LOGW("Input key is empty.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->Delete(key);
}

ErrCode AnsNotification::RemoveNotifications(const std::string &bundleName)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->DeleteByBundle(bundleName);
}

ErrCode AnsNotification::RemoveNotifications()
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->DeleteAll();
}

ErrCode AnsNotification::GetNotificationSlotsForBundle(
    const std::string &bundleName, std::vector<sptr<NotificationSlot>> &slots)
{
    if (bundleName.empty()) {
        ANS_LOGE("Input bundleName is empty.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSlotsByBundle(bundleName, slots);
}

ErrCode AnsNotification::GetAllActiveNotifications(std::vector<sptr<Notification>> &notification)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetAllActiveNotifications(notification);
}

ErrCode AnsNotification::GetAllActiveNotifications(
    const std::vector<std::string> key, std::vector<sptr<Notification>> &notification)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetSpecialActiveNotifications(key, notification);
}

ErrCode AnsNotification::IsAllowedNotify(const std::string &bundle, bool &allowed)
{
    if (bundle.empty()) {
        ANS_LOGE("Input bundle is empty.");
        return ERR_ANS_INVALID_PARAM;
    }

    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->IsSpecialBundleAllowedNotify(bundle, allowed);
}

ErrCode AnsNotification::SetNotificationsEnabledForAllBundles(const std::string &deviceId, bool enabled)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetNotificationsEnabledForAllBundles(deviceId, enabled);
}

ErrCode AnsNotification::SetNotificationsEnabledForDefaultBundle(const std::string &deviceId, bool enabled)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetNotificationsEnabledForBundle(deviceId, enabled);
}

ErrCode AnsNotification::SetNotificationsEnabledForSpecifiedBundle(
    const std::string &bundle, const std::string &deviceId, bool enabled)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetNotificationsEnabledForSpecialBundle(deviceId, bundle, enabled);
}

ErrCode AnsNotification::SetDisturbMode(NotificationConstant::DisturbMode mode)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->SetDisturbMode(mode);
}

ErrCode AnsNotification::GetDisturbMode(NotificationConstant::DisturbMode &disturbMode)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->GetDisturbMode(disturbMode);
}

void AnsNotification::ResetAnsManagerProxy()
{
    ANS_LOGI("enter");
    std::lock_guard<std::mutex> lock(mutex_);
    if ((ansManagerProxy_ != nullptr) && (ansManagerProxy_->AsObject() != nullptr)) {
        ansManagerProxy_->AsObject()->RemoveDeathRecipient(recipient_);
    }
    ansManagerProxy_ = nullptr;
}

ErrCode AnsNotification::ShellDump(const std::string &dumpOption, std::vector<std::string> &dumpInfo)
{
    if (!GetAnsManagerProxy()) {
        ANS_LOGE("GetAnsManagerProxy fail.");
        return ERR_ANS_SERVICE_NOT_CONNECTED;
    }
    return ansManagerProxy_->ShellDump(dumpOption, dumpInfo);
}

bool AnsNotification::GetAnsManagerProxy()
{
    if (!ansManagerProxy_) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ansManagerProxy_) {
            sptr<ISystemAbilityManager> systemAbilityManager =
                SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
            if (!systemAbilityManager) {
                ANS_LOGE("Failed to get system ability mgr.");
                return false;
            }

            sptr<IRemoteObject> remoteObject =
                systemAbilityManager->GetSystemAbility(ADVANCED_NOTIFICATION_SERVICE_ABILITY_ID);
            if (!remoteObject) {
                ANS_LOGE("Failed to get notification Manager.");
                return false;
            }

            ansManagerProxy_ = iface_cast<IAnsManager>(remoteObject);
            if ((!ansManagerProxy_) || (!ansManagerProxy_->AsObject())) {
                ANS_LOGE("Failed to get notification Manager's proxy");
                return false;
            }

            recipient_ = new AnsManagerDeathRecipient();
            if (!recipient_) {
                ANS_LOGE("Failed to create death recipient");
                return false;
            }
            ansManagerProxy_->AsObject()->AddDeathRecipient(recipient_);
        }
    }

    return true;
}

bool AnsNotification::CanPublishMediaContent(const NotificationRequest &request) const
{
    if (NotificationContent::Type::MEDIA != request.GetNotificationType()) {
        return true;
    }

    if (request.GetContent() == nullptr) {
        ANS_LOGE("Failed to publish notification with null content.");
        return false;
    }

    auto media = std::static_pointer_cast<NotificationMediaContent>(request.GetContent()->GetNotificationContent());
    if (media == nullptr) {
        ANS_LOGE("Failed to get media content.");
        return false;
    }

    auto showActions = media->GetShownActions();
    uint32_t size = request.GetActionButtons().size();
    for (auto it = showActions.begin(); it != showActions.end(); ++it) {
        if (*it > size) {
            ANS_LOGE("The sequence numbers actions is: %{public}d, the assigned to added action buttons size is: "
                     "%{public}d.",
                *it,
                size);
            return false;
        }
    }

    return true;
}
}  // namespace Notification
}  // namespace OHOS