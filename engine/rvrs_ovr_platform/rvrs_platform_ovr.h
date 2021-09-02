#pragma once

#define kInviteTimeout 60

typedef enum rvrs_userFlagBits
{
	kUserUnknown = 0,
	kUserOnline = 0x1,
	kUserHost = 0x2,
	kUserJoined = 0x4,
	kUserInvited = 0x8,
	kUserInviteReceived = 0x10,
	kUserMuted = 0x20,
}rvrs_userFlagBits;

typedef enum rvrp_roomCallbackType
{
	kRoomCallbackUnknown = 0,
	kRoomCallbackCreate,
	kRoomCallbackUpdateFriends,
	kRoomCallbackInviteReceived,
	kRoomCallbackJoinRoom,
	kRoomCallbackUpdateRoom,
	kRoomCallbackUserKicked,
	kRoomCallbackUserLeft,
}rvrp_roomCallbackType;

typedef enum rvrp_netCallbackType
{
	kNetCallbackUnknown,
	kNetCallbackConnectionState,
	kNetCallbackPing,
}rvrp_netCallbackType;

typedef enum rvrp_voipCallbackType
{
	kVoipCallbackUnknown,
}rvrp_voipCallbackType;

typedef enum rvrp_netState
{
	kNetStateUnknown,
	kNetStateConnected,
	kNetStateTimeout,
	kNetStateClosed
}rvrp_netState;

typedef enum rvrp_netSendPolicy
{
	kNetSendUnreliable,
	kNetSendReliable
}rvrp_netSendPolicy;

typedef enum rvrp_voipState
{
	kVoipStateUnknown,
	kVoipStateConnected,
	kVoipStateTimeout,
	kVoipStateClosed
}rvrp_voipState;

typedef struct rvrs_userOvr
{
	uint64_t userId;					//Oculus userId.
	uint64_t roomId;					//Oculus roomId (i.e. room the user is in).
	uint32_t hashId;					//Hashed userId.
	uint32_t flags;	
	uint64_t lastPing;
	int64_t lastPacketTimestamp;
	int64_t inviteTime;
	int32_t sequence;
	rvrp_netState netState;
	rvrp_voipState voipState;
	union {
		const char* userName;			//Pointer to userName if using a known fixed address.
		int32_t userNameBufferIndex;	//Index pointing to userName if in a re-allocatable buffer. 
	};
	const void* voipStream;
	const char* presence;
	const char* inviteToken;
}rvrs_userOvr;

typedef struct rvrs_roomCallbackData
{
	uint64_t					roomID;
	rvrp_roomCallbackType		type;
	uint64_t					inviteFromUserID;
	size_t						numInvitableUsers;
	rvrs_userOvr*				invitableUsers;
	size_t						numJoinedUsers;
	rvrs_userOvr*				joinedUsers;
}rvrs_roomCallbackData;

typedef struct rvrs_netCallbackData
{
	uint64_t				userId;
	rvrp_netCallbackType	type;
	union {
		rvrp_netState		netState;
		uint64_t			ping;
	};
}rvrs_netCallbackData;

typedef struct rvrs_voipCallbackData
{
	uint64_t				userId;
	rvrp_voipCallbackType	type;
	rvrp_voipState			voipState;
	int32_t					bufferSize;
}rvrs_voipCallbackData;

typedef struct rvrp_netPacketHeader
{
	uint32_t	packetType;
	uint32_t	flags;
}rvrp_netPacketHeader;

typedef enum rvrp_netPacketFlags
{
	kNPNone = 0,
	kNPNeedsAck,
	kNPMaxFlags = 0xffff,
}rvrp_netPacketFlags;

typedef struct rvrp_netPacketDesc
{
	uint64_t userId;
	size_t packetSize;
	const void* packetData;
	rvrp_netSendPolicy sendPolicy;
//	int32_t timeOut;
}rvrp_netPacketDesc;


typedef void rvrs_platformCallback(uint64_t userId);
typedef void rvrs_roomCallback(const rvrs_roomCallbackData* roomData);
typedef void rvrs_netCallback(const rvrs_netCallbackData* netData);
typedef void rvrs_voipCallback(const rvrs_voipCallbackData* voipData);

typedef struct rvrs_platformDescOvr
{
	const char* appId;
	rvrs_platformCallback^ entitledCallback;
}rvrs_platformDescOvr;

typedef struct rvrs_ovrCreateRoomDesc
{
	uint32_t maxUsers;
	rvrs_roomCallback^ roomCallback;
}rvrs_ovrCreateRoomDesc;

typedef struct rvrs_ovrInviteUserDesc
{
	uint64_t roomID;
	const char* inviteToken;
//	rvrs_createRoomCallback^ createRoomCallback;
}rvrs_ovrInviteUserDesc;

typedef struct rvrs_ovrNetConnectDesc
{
	uint64_t userId;
	rvrs_netCallback^ netCallback;
}rvrs_ovrNetConnectDesc;

typedef struct rvrs_ovrVoipStartDesc
{
	uint64_t userId;
	rvrs_voipCallback^ voipCallback;
}rvrs_ovrVoipStartDesc;

void rvrs_initializePlatformOvr(const rvrs_platformDescOvr* desc);
void rvrs_terminateOvrPlatform();
bool rvrs_processOvrMessages();
void rvrs_createPrivateRoomOvr(const rvrs_ovrCreateRoomDesc* desc);
void rvrs_inviteUserOvr(const rvrs_ovrInviteUserDesc* desc);
void rvrs_kickUserOvr(uint64_t roomID, uint64_t userID);
void rvrs_joinRoomOvr(uint64_t roomId);
void rvrs_leaveRoomOvr(uint64_t roomId);
void rvrs_netConnect(const rvrs_ovrNetConnectDesc* desc);
void rvrs_netClose(const uint64_t userId);
void rvrs_netSendPacket(const rvrp_netPacketDesc* desc);
int32_t rvrs_netReadPackets(rvrp_netPacketDesc* packets, uint8_t* packetBuffer, const int32_t maxPackets);
void rvrs_netPing(uint64_t userId);
void rvrs_voipStart(const rvrs_ovrVoipStartDesc* desc);
void rvrs_voipStop(const uint64_t userId);
void rvrs_voipMute(bool muted);
size_t rvrs_voipGetPCM(const uint64_t userId, int16_t* voipBuffer, size_t voipBufferSize);
