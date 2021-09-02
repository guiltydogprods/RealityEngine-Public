#pragma once
#include "math/vec4.h"
#include "math/mat4x4.h"

typedef struct rvrs_avatarOvr { int32_t id; } rvrs_avatarOvr;
typedef struct rvrs_avatarInstance { int32_t id; } rvrs_avatarInstance;
typedef struct rvrs_avatarBody { int32_t id; } rvrs_avatarBody;
typedef struct rvrs_avatarHands { int32_t id; } rvrs_avatarHands;
typedef struct rvrs_avatarFrameParams rvrs_avatarFrameParams;

typedef struct ovrAvatar_ ovrAvatar;
typedef struct ovrVector3f_ ovrVector3f;
typedef struct ovrQuatf_ ovrQuatf;


void rvrs_initializeAvatarOvr(const char* appId);
void rvrs_terminateAvatarOvr();
bool rvrs_requestAvatarSpecOvr(uint64_t userId, int32_t slot);
bool rvrs_processAvatarMessagesOvr();

rvrs_avatarOvr rvrs_createAvatarOvr(uint64_t userId, const ovrAvatar* ovrAvatar);
void rvrs_updateAvatarInstance(int32_t instanceIdx, float dt, const rvrs_avatarFrameParams* params);
void rvrs_renderAvatarOvr(int32_t eye, const rvrs_avatarFrameParams* params);
rvrs_avatarOvr rvrs_getAvatar();