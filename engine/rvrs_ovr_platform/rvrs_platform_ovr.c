#include "stdafx.h"
#include "rvrs_platform_ovr.h"
#include "rvrs_avatar_ovr.h"
#include "system.h"

#if 1 //def VRSYSTEM_OCULUS_WIN
#include "OVR_Platform.h"
#include "OVR_Avatar.h"

#ifndef RE_PLATFORM_WIN64
#include "Block.h"
#else
extern void* (*_Block_copy)(const void* aBlock);
extern void (*_Block_release)(const void* aBlock);

#define Block_copy(...) ((__typeof(__VA_ARGS__))_Block_copy((const void *)(__VA_ARGS__)))
#define Block_release(...) _Block_release((const void *)(__VA_ARGS__))
#endif

typedef struct rvrs_platformDataOvr
{
	const char* appId;
	uint64_t userId;
	ovrAvatar* avatar;
	ovrRoomHandle roomHandle;
	ovrID roomID;
	ovrID kickedUserID;
	time_t initializationTime;
	rvrs_platformCallback^ entitledCallback;
	rvrs_roomCallback^ roomCallback;
	rvrs_netCallback^ netCallback;
	rvrs_voipCallback^ voipCallback;
	bool bPlatformInitialized;
	bool bAvatarSpecRequested;
}rvrs_platformDataOvr;

static rvrs_platformDataOvr s_data = { 0 };

#ifndef VRSYSTEM_OCULUS_WIN
extern ovrApp* s_appState;
#endif

void processPlatformInitializeAsynchronous(ovrMessageHandle message);
void processGetEntitlement(ovrMessageHandle message);
void processCreateAndJoinPrivateRoom(ovrMessageHandle message);
void processGetInvitableUsers(ovrMessageHandle message);
void processInviteUser(ovrMessageHandle message);
void processInvitesReceivedNotification(ovrMessageHandle message);
void processInviteReceivedNotification(ovrMessageHandle message);
void processJoinRoom(ovrMessageHandle message);
void processLeaveRoom(ovrMessageHandle message);
void processKickUser(ovrMessageHandle message);
void processRoomUpdateNotification(ovrMessageHandle message);
void processNetworkingPeerConnect(ovrMessageHandle message);
void processNetworkingStateChange(ovrMessageHandle message);
void processNetworkingPingResult(ovrMessageHandle message);
void processVoipStateChange(ovrMessageHandle message);

void rvrs_initializePlatformOvr(const rvrs_platformDescOvr* desc)
{
	memset(&s_data, 0, sizeof(rvrs_platformDataOvr));
	rvrs_initializeAvatarOvr(NULL);
	if (desc != NULL)
	{

#ifdef VRSYSTEM_OCULUS_WIN
		ovrPlatformInitializeResult outResult;
		if (ovr_PlatformInitializeWindowsAsynchronous(desc->appId, &outResult) != ovrPlatformInitialize_Success)
		{
			// Exit.  Initialization failed which means either the oculus service isn’t
			// on the machine or they’ve hacked their DLL
		}
#else
		if (s_appState != NULL) {
			uint64_t PlatformInitializeRequest = ovr_PlatformInitializeAndroidAsynchronous(desc->appId, s_appState->Java.ActivityObject, s_appState->Java.Env);
		}
#endif
		s_data.appId = desc->appId;
		s_data.entitledCallback = Block_copy(desc->entitledCallback);
	}
}

void rvrs_terminateOvrPlatform()
{
	rvrs_terminateAvatarOvr();
	memset(&s_data, 0, sizeof(rvrs_platformDataOvr));
}

bool rvrs_processOvrMessages()
{
	bool renderOnReturn = true;

	ovrMessageHandle message;
	// Poll for a response
	while ((message = ovr_PopMessage()) != NULL)
	{
//		rsys_print("OvrMessage: type 0x%08x\n", ovr_Message_GetType(message));
		switch (ovr_Message_GetType(message))
		{
		case ovrMessage_PlatformInitializeWindowsAsynchronous:
		case ovrMessage_PlatformInitializeAndroidAsynchronous:
			processPlatformInitializeAsynchronous(message);
			break;
		case ovrMessage_Entitlement_GetIsViewerEntitled:
			processGetEntitlement(message);
			break;
		case ovrMessage_Room_CreateAndJoinPrivate2:
			processCreateAndJoinPrivateRoom(message);
			break;
		case ovrMessage_Room_GetInvitableUsers2:
			processGetInvitableUsers(message);
			break;
		case ovrMessage_Room_InviteUser:
			processInviteUser(message);
			break;
		case ovrMessage_Room_KickUser:
			processKickUser(message);
			break;
		case ovrMessage_Room_Join:
		case ovrMessage_Room_Join2:
			processJoinRoom(message);
			break;
		case ovrMessage_Room_Leave:
			processLeaveRoom(message);
			break;
		case ovrMessage_Notification_GetRoomInvites:
			processInvitesReceivedNotification(message);
			break;
		case ovrMessage_Notification_Room_InviteReceived:
			processInviteReceivedNotification(message);
			break;
		case ovrMessage_Notification_Room_RoomUpdate:
			processRoomUpdateNotification(message);
			break;
		case ovrMessage_Notification_Networking_PeerConnectRequest:
			processNetworkingPeerConnect(message);
			break;
		case ovrMessage_Notification_Networking_ConnectionStateChange:
			processNetworkingStateChange(message);
			break;
		case ovrMessage_Notification_Networking_PingResult:
			processNetworkingPingResult(message);
			break;
		case ovrMessage_Notification_Voip_ConnectRequest:
//			processVoipPeerConnect(message);
			rsys_print("WARNING: Received - ovrMessage_Notification_Voip_ConnectRequest\n");
//			assert(0);
			break;
		case ovrMessage_Notification_Voip_StateChange:
			processVoipStateChange(message);
			break;
		default:
			break;
		}
		ovr_FreeMessage(message);
	}

	rvrs_processAvatarMessagesOvr();

	return renderOnReturn;
}
#endif

void rvrs_createPrivateRoomOvr(const rvrs_ovrCreateRoomDesc* desc)
{
	if (s_data.roomCallback != NULL) {
		Block_release(s_data.roomCallback);
	}
	s_data.roomCallback = Block_copy(desc->roomCallback);
	ovrRequest req = ovr_Room_CreateAndJoinPrivate2(ovrRoom_JoinPolicyInvitedUsers, desc->maxUsers, NULL);
}

void rvrs_inviteUserOvr(const rvrs_ovrInviteUserDesc* desc)
{
	ovrRequest req = ovr_Room_InviteUser(desc->roomID, desc->inviteToken);
	AssertMsg(req != invalidRequestID, "ovr_Room_InviteUser - returned invalidRequestID!\n");
}

void rvrs_kickUserOvr(uint64_t roomID, uint64_t userID)
{
	AssertMsg(roomID == s_data.roomID, "Incorrect roomID\n");

	const int32_t kickDurationSeconds = 10;
	s_data.kickedUserID = userID;

	ovr_Room_KickUser(roomID, userID, kickDurationSeconds);
}

void rvrs_joinRoomOvr(uint64_t roomId)
{
	ovrRequest req = ovr_Room_Join2(roomId, NULL);
	AssertMsg(req != invalidRequestID, "ovr_Room_Join2 - returned invalidRequestID!\n");
}

void rvrs_leaveRoomOvr(uint64_t roomId)
{
	ovrRequest req = ovr_Room_Leave(roomId);
	AssertMsg(req != invalidRequestID, "ovr_Room_Leave - returned invalidRequestID!\n");
}

void rvrs_netConnect(const rvrs_ovrNetConnectDesc* desc)
{
	if (s_data.netCallback == NULL) {
		s_data.netCallback = Block_copy(desc->netCallback);
	}
	ovr_Net_Connect(desc->userId);
}

void rvrs_netClose(const uint64_t userId)
{
	if (userId != 0) {
		ovr_Net_Close(userId);
	}
	else {
		ovr_Net_CloseForCurrentRoom();
	}
	if (s_data.netCallback != NULL) {
		Block_release(s_data.netCallback);
		s_data.netCallback = NULL;
	}
}

void rvrs_netSendPacket(const rvrp_netPacketDesc* desc)
{
	AssertMsg(desc != NULL, "netSendPacket: Missing packet desc");
	ovrSendPolicy nativeSendPolicy = desc->sendPolicy == kNetSendUnreliable ? ovrSend_Unreliable : ovrSend_Reliable;
	if (desc->userId != 0) {
		ovr_Net_SendPacket(desc->userId, desc->packetSize, desc->packetData, nativeSendPolicy);
	} else {
		ovr_Net_SendPacketToCurrentRoom(desc->packetSize, desc->packetData, nativeSendPolicy);
	}
/*
	if (nativeSendPolicy == ovrSend_Unreliable && desc->timeOut > 0)
	{
		const rvrp_netPacketHeader* packetHeader = desc->packetData;
		packetHeader->
	}
*/
}

int32_t rvrs_netReadPackets(rvrp_netPacketDesc* packets, uint8_t* packetBuffer, const int32_t maxPackets)
{
	ovrPacketHandle packet;
	int32_t packetCount = 0;
	uint8_t* dstPtr = packetBuffer;
	static int32_t packetsReceived = 0;
	while ((packet = ovr_Net_ReadPacket()) != NULL)
	{
		ovrID sender = ovr_Packet_GetSenderID(packet);
		size_t packetSize = ovr_Packet_GetSize(packet);
		const void* packetBytes = ovr_Packet_GetBytes(packet);
/*
		if (packetsReceived > 10) {
			float discard = (float)rand() / (float)RAND_MAX;
			if (discard > 0.8f) {
				ovr_Packet_Free(packet);
				continue;
			}
		}
*/
		if (packetCount < maxPackets)
		{
			memcpy(dstPtr, packetBytes, packetSize);
			packets[packetCount].userId = sender;
			packets[packetCount].packetSize = packetSize;
			packets[packetCount].packetData = dstPtr;
			packetCount++;
			dstPtr += packetSize;
			packetsReceived++;
		}
		ovr_Packet_Free(packet);
	}
	return packetCount;
}

void rvrs_netPing(uint64_t userId)
{
	ovrRequest res = ovr_Net_Ping(userId);
	AssertMsg(res != invalidRequestID, "ovr_Net_Ping - returned invalidRequestID!\n");
}

void rvrs_voipStart(const rvrs_ovrVoipStartDesc* desc)
{
	if (s_data.voipCallback == NULL) {
		s_data.voipCallback = Block_copy(desc->voipCallback);
	}
	ovr_Voip_Start(desc->userId);
}

void rvrs_voipStop(const uint64_t userId)
{
	ovr_Voip_Stop(userId);
}

void rvrs_voipMute(bool muted)
{
	ovr_Voip_SetMicrophoneMuted(muted == true ? ovrVoipMuteState_Muted : ovrVoipMuteState_Unmuted);
}

size_t rvrs_voipGetPCM(const uint64_t userId, int16_t* voipBuffer, size_t voipBufferSize)
{
	return ovr_Voip_GetPCM(userId, voipBuffer, voipBufferSize);
}

void processPlatformInitializeAsynchronous(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		rsys_print("Ovr Platform Initialized successfully.\n");
		s_data.bPlatformInitialized = true;
		ovr_Entitlement_GetIsViewerEntitled();
		s_data.initializationTime = time(NULL);
	}
	else
	{
		rsys_print("Ovr Platform Initialization failed!\n");
	}
}
void processGetEntitlement(ovrMessageHandle message)
{
	uint64_t userId = 0;
	if (!ovr_Message_IsError(message))
	{
		rsys_print("User is entitled.\n");
		userId = ovr_GetLoggedInUserID();
	}
	else
	{
		rsys_print("User is NOT entitled!\n");
	}
	s_data.userId = userId;
	if (s_data.entitledCallback != NULL) {
		s_data.entitledCallback(userId);
		Block_release(s_data.entitledCallback);
		s_data.entitledCallback = NULL;
	}
}


void processCreateAndJoinPrivateRoom(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		s_data.roomHandle = ovr_Message_GetRoom(message);
		s_data.roomID = ovr_Room_GetID(s_data.roomHandle);

		rsys_print("Created and joined private room %" PRId64 "\n", s_data.roomID);

		ovrRoomOptionsHandle roomOptions = ovr_RoomOptions_Create();
		ovr_RoomOptions_SetOrdering(roomOptions, ovrUserOrdering_PresenceAlphabetical);
		ovr_RoomOptions_SetRoomId(roomOptions, s_data.roomID);
		ovr_Room_GetInvitableUsers2(roomOptions);
		ovr_RoomOptions_Destroy(roomOptions);
		if (s_data.roomCallback != NULL) {
			s_data.roomCallback(&(rvrs_roomCallbackData) {
				.type = kRoomCallbackCreate,
				.roomID = s_data.roomID,
			});
		}
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error creating private room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processGetInvitableUsers(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovr_Notification_GetRoomInvites();													// Query existing invites.
		rsys_print("Received get friends successfully.\n");
		ovrUserArrayHandle users = ovr_Message_GetUserArray(message);
		size_t numUsers = ovr_UserArray_GetSize(users);
		rvrs_userOvr* friendsList = (rvrs_userOvr*)alloca(sizeof(rvrs_userOvr) * numUsers);
		memset(friendsList, 0, sizeof(rvrs_userOvr) * numUsers);
		for (size_t i = 0; i < numUsers; ++i)
		{
			ovrUserHandle user = ovr_UserArray_GetElement(users, i);
			ovrID id = ovr_User_GetID(user);
			ovrUserPresenceStatus status = ovr_User_GetPresenceStatus(user);
			const char* oculusID = ovr_User_GetOculusID(user);
			const char* presence = ovr_User_GetPresence(user);
			const char* inviteToken = ovr_User_GetInviteToken(user);
			friendsList[i].userId = id;
			friendsList[i].flags = status == ovrUserPresenceStatus_Online ? kUserOnline : 0;
			friendsList[i].userName = oculusID;
			friendsList[i].presence = presence;
			friendsList[i].inviteToken = inviteToken;
		}
		if (s_data.roomCallback != NULL) {
			s_data.roomCallback(&(rvrs_roomCallbackData) {
				.type = kRoomCallbackUpdateFriends,
				.roomID = s_data.roomID,
				.numInvitableUsers = numUsers,
				.invitableUsers = friendsList
			});
		}
	}
	else 
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Received get friends failure: %s\n", ovr_Error_GetMessage(error));
	}
}

void processInviteUser(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		rsys_print("Invite sent!\n");
		ovrRoomHandle roomInMessage = ovr_Message_GetRoom(message);
		ovrID roomID = ovr_Room_GetID(roomInMessage);
		AssertMsg((roomID == s_data.roomID), "Room referenced in message is not the current room?\n");
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error inviting users to room: %s\n", ovr_Error_GetMessage(error));
	}
}

void handleInviteNotification(ovrRoomInviteNotificationHandle roomInviteNotification)
{
	if (roomInviteNotification != NULL)
	{
		ovrID senderID = ovr_RoomInviteNotification_GetSenderID(roomInviteNotification);
		ovrID roomID = ovr_RoomInviteNotification_GetRoomID(roomInviteNotification);
		uint64_t inviteTime = ovr_RoomInviteNotification_GetSentTime(roomInviteNotification);
		if (inviteTime < s_data.initializationTime)
		{
			time_t currentTime = time(NULL);
			if ((currentTime - inviteTime) > kInviteTimeout) {
				return;
			}
		}

		if (s_data.roomCallback != NULL) {
			s_data.roomCallback(&(rvrs_roomCallbackData) {
				.type = kRoomCallbackInviteReceived,
				.roomID = roomID,
				.inviteFromUserID = senderID
			});
		}
	}
}


void processInvitesReceivedNotification(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrRoomInviteNotificationArrayHandle notificationArrayInMessage = ovr_Message_GetRoomInviteNotificationArray(message);
		size_t numInvites = ovr_RoomInviteNotificationArray_GetSize(notificationArrayInMessage);
		if (numInvites > 0)
		{
			rsys_print("Invite(s) received!\n");
			for (size_t invite = 0; invite < numInvites; ++invite)
			{
				handleInviteNotification(ovr_RoomInviteNotificationArray_GetElement(notificationArrayInMessage, invite));
			}
		}
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error inviting users to room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processInviteReceivedNotification(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		rsys_print("Invite received!\n");
		handleInviteNotification(ovr_Message_GetRoomInviteNotification(message));
		// CLR - Automatically join room. No longer required but left in as useful for testing.
		//ovr_Room_Join(roomID, true);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error inviting users to room: %s\n", ovr_Error_GetMessage(error));
	}
}

void handleRoomUserUpdate(ovrRoomHandle room, rvrp_roomCallbackType callbackType)
{
	if (room != 0)
	{
		ovrID roomID = ovr_Room_GetID(room);
		ovrUserHandle roomOwnerHandle = ovr_Room_GetOwner(room);
		ovrID roomOwner = ovr_User_GetID(roomOwnerHandle);
		ovrUserArrayHandle roomUsers = ovr_Room_GetUsers(room);
		size_t numUsers = ovr_UserArray_GetSize(roomUsers);
		rsys_print("Room update! %d users\n", numUsers);
		rvrs_userOvr* userList = (rvrs_userOvr*)alloca(sizeof(rvrs_userOvr) * numUsers);
		memset(userList, 0, sizeof(rvrs_userOvr) * numUsers);
		for (size_t i = 0; i < numUsers; ++i)
		{
			ovrUserHandle user = ovr_UserArray_GetElement(roomUsers, i);
			ovrID id = ovr_User_GetID(user);
			ovrUserPresenceStatus status = ovr_User_GetPresenceStatus(user);
			const char* oculusID = ovr_User_GetOculusID(user);
			const char* presence = ovr_User_GetPresence(user);
			const char* inviteToken = ovr_User_GetInviteToken(user);
			userList[i].userId = id;
			userList[i].flags = status == ovrUserPresenceStatus_Online ? kUserOnline : 0;
			userList[i].flags |= id == roomOwner ? kUserHost : kUserJoined;
			userList[i].userName = oculusID;
			userList[i].presence = presence;
			userList[i].inviteToken = inviteToken;
		}
		if (s_data.roomCallback != NULL) {
			s_data.roomCallback(&(rvrs_roomCallbackData) {
				.type = callbackType,
				.roomID = roomID,
				.numJoinedUsers = numUsers,
				.joinedUsers = userList
			});
		}
	}
}

void processJoinRoom(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrRoomHandle roomInMessage = ovr_Message_GetRoom(message);
		handleRoomUserUpdate(roomInMessage, kRoomCallbackJoinRoom);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error inviting users to room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processLeaveRoom(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrRoomHandle roomInMessage = ovr_Message_GetRoom(message);
		handleRoomUserUpdate(roomInMessage, kRoomCallbackUserLeft);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error leaving room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processKickUser(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		rsys_print("KickUser received!\n");
		ovrRoomHandle roomInMessage = ovr_Message_GetRoom(message);
		handleRoomUserUpdate(roomInMessage, kRoomCallbackUserKicked);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error kicking user from room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processRoomUpdateNotification(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrRoomHandle roomInMessage = ovr_Message_GetRoom(message);
		handleRoomUserUpdate(roomInMessage, kRoomCallbackUpdateRoom);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error inviting users to room: %s\n", ovr_Error_GetMessage(error));
	}
}

void processNetworkingPeerConnect(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrNetworkingPeerHandle peerInMessdage = ovr_Message_GetNetworkingPeer(message);
		ovrID netUserId = ovr_NetworkingPeer_GetID(peerInMessdage);
		ovrPeerConnectionState connectionState = ovr_NetworkingPeer_GetState(peerInMessdage);
		if (!ovr_Net_IsConnected(netUserId)) {
			ovr_Net_Accept(netUserId);
		}
		rsys_print("NetConnect: Connection with user %" PRId64 " accepted!\n", netUserId);
	}
	else
	{
		const ovrErrorHandle error = ovr_Message_GetError(message);
		rsys_print("Error with peer connect request: %s\n", ovr_Error_GetMessage(error));
	}
}

void processNetworkingStateChange(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrNetworkingPeerHandle peerInMessage = ovr_Message_GetNetworkingPeer(message);
		ovrPeerConnectionState connectionState = ovr_NetworkingPeer_GetState(peerInMessage);
		ovrID netUserId = ovr_NetworkingPeer_GetID(peerInMessage);
		rvrp_netState netState = kNetStateUnknown;
		switch (connectionState)
		{
		case ovrPeerState_Connected:
			ovr_Net_Ping(netUserId);
			netState = kNetStateConnected;
			break;
		case ovrPeerState_Timeout:
			netState = kNetStateTimeout;
			break;
		case ovrPeerState_Closed:
			netState = kNetStateClosed;
			break;
		default:
			assert(0);
		}
		if (s_data.netCallback != NULL) {
			s_data.netCallback(&(rvrs_netCallbackData) {
				.userId = netUserId,
				.type = kNetCallbackConnectionState,
				.netState = netState,
			});
		}
	}
}

void processNetworkingPingResult(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		ovrPingResultHandle pingInMessage = ovr_Message_GetPingResult(message);
		uint64_t pingID = ovr_PingResult_GetID(pingInMessage);
		uint64_t pingUsec = ovr_PingResult_GetPingTimeUsec(pingInMessage);
		if (s_data.netCallback != NULL) {
			s_data.netCallback(&(rvrs_netCallbackData) {
				.userId = pingID,
				.type = kNetCallbackPing,
				.ping = pingUsec,
			});
		}
	}
}

void processVoipStateChange(ovrMessageHandle message)
{
	if (!ovr_Message_IsError(message))
	{
		size_t maxBufferSize = 0;
		ovrNetworkingPeerHandle peerInMessage = ovr_Message_GetNetworkingPeer(message);
		ovrPeerConnectionState connectionState = ovr_NetworkingPeer_GetState(peerInMessage);
		ovrID netUserId = ovr_NetworkingPeer_GetID(peerInMessage);
		rvrp_voipState voipState = kVoipStateUnknown;
		switch (connectionState)
		{
		case ovrPeerState_Connected:
			voipState = kVoipStateConnected;
			maxBufferSize = ovr_Voip_GetOutputBufferMaxSize();
//			rsys_print("VOIP: Max buffer size = %d\n", maxBufferSize);
//			ovr_Voip_SetMicrophoneMuted(ovrVoipMuteState_Unmuted);
//			ovrVoipMuteState micState = ovr_Voip_GetSystemVoipMicrophoneMuted();
//			rsys_print("VOIP: Mic is %s \n", micState == ovrVoipMuteState_Unknown ? "unknown" : micState == ovrVoipMuteState_Muted ? "muted" : "unmuted");
			break;
		case ovrPeerState_Timeout:
			voipState = kVoipStateTimeout;
			break;
		case ovrPeerState_Closed:
			voipState = kVoipStateClosed;
			break;
		default:
			assert(0);
		}
		if (s_data.voipCallback != NULL) {
			s_data.voipCallback(&(rvrs_voipCallbackData) {
				.userId = netUserId,
//				.type = kVoipCallbackTypeUnknown,
				.voipState = voipState,
				.bufferSize = maxBufferSize,
			});
		}
	}
}
