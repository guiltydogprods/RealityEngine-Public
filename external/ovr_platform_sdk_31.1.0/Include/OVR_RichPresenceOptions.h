// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_RICH_PRESENCE_OPTIONS_H
#define OVR_RICH_PRESENCE_OPTIONS_H

#include "OVR_Platform_Defs.h"
#include "OVR_Types.h"
#include <stddef.h>
#include <stdbool.h>

#include "OVR_RichPresenceExtraContext.h"

struct ovrRichPresenceOptions;
typedef struct ovrRichPresenceOptions* ovrRichPresenceOptionsHandle;

OVRP_PUBLIC_FUNCTION(ovrRichPresenceOptionsHandle) ovr_RichPresenceOptions_Create();
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_Destroy(ovrRichPresenceOptionsHandle handle);
/// This the unique API Name that refers to an in-app destination
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetApiName(ovrRichPresenceOptionsHandle handle, const char * value);
/// DEPRECATED: Unused
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetArgsString(ovrRichPresenceOptionsHandle handle, const char* key, const char* value);
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_ClearArgs(ovrRichPresenceOptionsHandle handle);
/// The current amount of users that have joined this user's
/// squad/team/game/match etc.
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetCurrentCapacity(ovrRichPresenceOptionsHandle handle, unsigned int value);
/// Optionally passed in to use a different deeplink message than the one
/// defined in the api_name
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetDeeplinkMessageOverride(ovrRichPresenceOptionsHandle handle, const char * value);
/// The time the current match/game/round etc. ends
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetEndTime(ovrRichPresenceOptionsHandle handle, unsigned long long value);
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetExtraContext(ovrRichPresenceOptionsHandle handle, ovrRichPresenceExtraContext value);
/// Users reported with the same instance ID will be considered to be together
/// and could interact with each other. Renamed to
/// ovr_RichPresenceOptions_SetInstanceId
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetInstanceId(ovrRichPresenceOptionsHandle handle, const char * value);
/// Set whether or not the person is shown as active or idle
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetIsIdle(ovrRichPresenceOptionsHandle handle, bool value);
/// Set whether or not the person is shown as joinable or not to others
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetIsJoinable(ovrRichPresenceOptionsHandle handle, bool value);
/// DEPRECATED: unused
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetJoinableId(ovrRichPresenceOptionsHandle handle, const char * value);
/// This is a session that represents a group/squad/party of users that are to
/// remain together across multiple rounds, matches, levels maps, game modes,
/// etc. Users with the same lobby session id in their rich presence will be
/// considered together.
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetLobbySessionId(ovrRichPresenceOptionsHandle handle, const char * value);
/// This is a session that represents all the users that are playing a specific
/// instance of a map, game mode, round, etc. This can include users from
/// multiple different lobbies that joined together and the users may or may
/// not remain together after the match is over. Users with the same match
/// session id in their rich presence is also considered to be together but
/// have a looser connection than those together in a lobby session.
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetMatchSessionId(ovrRichPresenceOptionsHandle handle, const char * value);
/// The maximum that can join this user
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetMaxCapacity(ovrRichPresenceOptionsHandle handle, unsigned int value);
/// The time the current match/game/round etc. started
OVRP_PUBLIC_FUNCTION(void) ovr_RichPresenceOptions_SetStartTime(ovrRichPresenceOptionsHandle handle, unsigned long long value);

#endif
