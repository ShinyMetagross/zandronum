//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: maprotation.cpp
//
// Description: The server's list of maps to play.
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <vector>
#include "c_cvars.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "m_random.h"
#include "maprotation.h"
#include "p_setup.h"
#include "joinqueue.h"
#include "sv_main.h"
#include "sv_commands.h"
#include "network.h"
#include "v_text.h"

//*****************************************************************************
//	VARIABLES

std::vector<MapRotationEntry>	g_MapRotationEntries;

static	ULONG					g_ulCurMapInList;
static	ULONG					g_ulNextMapInList;

// [AK] This is true when the next map should ignore its player limits.
static	bool					g_NextMapIgnoresLimits;

//*****************************************************************************
//	FUNCTIONS

void MAPROTATION_Construct( void )
{
	g_MapRotationEntries.clear( );
	g_ulCurMapInList = g_ulNextMapInList = 0;
	g_NextMapIgnoresLimits = false;

	// [AK] If we're the server, tell the clients to clear their map lists too.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DelFromMapRotation( NULL, true );
}

//*****************************************************************************
//
unsigned int MAPROTATION_CountEligiblePlayers( void )
{
	unsigned int playerCount = 0;

	// [AK] Count players who are already playing or are in the join queue.
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( playeringame[i] ) && (( PLAYER_IsTrueSpectator( &players[i] ) == false ) || ( JOINQUEUE_GetPositionInLine( i ) != -1 )))
			playerCount++;
	}

	return playerCount;
}

//*****************************************************************************
//
ULONG MAPROTATION_GetNumEntries( void )
{
	return g_MapRotationEntries.size( );
}

//*****************************************************************************
//
ULONG MAPROTATION_GetCurrentPosition( void )
{
	return ( g_ulCurMapInList );
}

//*****************************************************************************
//
ULONG MAPROTATION_GetNextPosition( void )
{
	return ( g_ulNextMapInList );
}

//*****************************************************************************
//
void MAPROTATION_SetCurrentPosition( ULONG ulPosition )
{
	if ( ulPosition >= g_MapRotationEntries.size( ))
		return;

	g_ulCurMapInList = ulPosition;
}

//*****************************************************************************
//
void MAPROTATION_SetNextPosition( unsigned int position, const bool ignoreLimits )
{
	if ( position >= g_MapRotationEntries.size( ))
		return;

	g_ulNextMapInList = position;
	g_NextMapIgnoresLimits = ignoreLimits;
}

//*****************************************************************************
//
bool MAPROTATION_ShouldNextMapIgnoreLimits( void )
{
	return g_NextMapIgnoresLimits;
}

//*****************************************************************************
//
bool MAPROTATION_CanEnterMap( ULONG ulIdx, ULONG ulPlayerCount )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return ( false );

	// [AK] If this is the next map in the rotation and it should ignore its
	// player limits because of the SetNextMapPosition ACS function, then it can
	// be entered regardless of whether or not the player count is admissable.
	if (( ulIdx == g_ulNextMapInList ) && ( g_NextMapIgnoresLimits ))
		return true;

	return (( g_MapRotationEntries[ulIdx].minPlayers <= ulPlayerCount ) && ( g_MapRotationEntries[ulIdx].maxPlayers >= ulPlayerCount ));
}

//*****************************************************************************
//
static bool MAPROTATION_MapHasLowestOrHighestLimit( ULONG ulIdx, ULONG ulLowest, ULONG ulHighest, bool bUseMax )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return ( false );

	if ( bUseMax )
		return ( g_MapRotationEntries[ulIdx].maxPlayers == ulHighest );
	else
		return ( g_MapRotationEntries[ulIdx].minPlayers == ulLowest );
}

//*****************************************************************************
//
static bool MAPROTATION_GetLowestAndHighestLimits( ULONG ulPlayerCount, ULONG &ulLowest, ULONG &ulHighest )
{
	bool bUseMaxLimit = false;
	ulLowest = MAXPLAYERS;
	ulHighest = 1;

	// [AK] Get the lowest min player limit and highest max player limit from the list.
	for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
	{
		if ( g_MapRotationEntries[i].minPlayers < ulLowest )
			ulLowest = g_MapRotationEntries[i].minPlayers;

		if ( g_MapRotationEntries[i].maxPlayers > ulHighest )
			ulHighest = g_MapRotationEntries[i].maxPlayers;

		// [AK] If there's any map where the player count exceeds the min limit, then use the max limit.
		if ( ulPlayerCount >= g_MapRotationEntries[i].minPlayers )
			bUseMaxLimit = true;
	}

	return ( bUseMaxLimit );
}

//*****************************************************************************
//
void MAPROTATION_CalcNextMap( const bool updateClients )
{
	if ( g_MapRotationEntries.empty( ))
		return;

	const ULONG ulPlayerCount = MAPROTATION_CountEligiblePlayers( );
	ULONG ulLowestLimit;
	ULONG ulHighestLimit;
	bool bUseMaxLimit;

	// [AK] Before determining the next map, make sure it won't ignore its limits.
	g_NextMapIgnoresLimits = false;

	// If all the maps have been played, make them all available again.
	{
		bool bAllMapsPlayed = true;
		for ( ULONG ulIdx = 0; ulIdx < g_MapRotationEntries.size( ); ulIdx++ )
		{
			// [AK] Ignore rotation entries that we can't select due to player limits.
			if ( MAPROTATION_CanEnterMap( ulIdx, ulPlayerCount ) == false )
				continue;

			if ( !g_MapRotationEntries[ulIdx].isUsed )
			{
				bAllMapsPlayed = false;
				break;
			}
		}
			
		if ( bAllMapsPlayed )
		{
			for ( ULONG ulIdx = 0; ulIdx < g_MapRotationEntries.size( ); ulIdx++ )
				g_MapRotationEntries[ulIdx].isUsed = false;

			// [AK] If we're the server, tell the clients to reset their map lists too.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_ResetMapRotation( );
		}
	}

	// [BB] The random selection is only necessary if there is more than one map.
	if ( sv_randommaprotation && ( g_MapRotationEntries.size( ) > 1 ) )
	{
		// Select a new map.
		std::vector<unsigned int> unusedEntries;
		for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); ++i )
		{
			// [AK] Only select maps that we can enter with the current number of players.
			if (( g_MapRotationEntries[i].isUsed == false ) && ( MAPROTATION_CanEnterMap( i, ulPlayerCount )))
				unusedEntries.push_back ( i );
		}

		// [AK] If we can't select any maps because the player count exceeds all limits, we'll just select the map with the lowest
		// lowest min player or highest max player limit, based on if there's too few or too many players.
		if ( unusedEntries.empty( ))
		{
			bUseMaxLimit = MAPROTATION_GetLowestAndHighestLimits( ulPlayerCount, ulLowestLimit, ulHighestLimit );
			for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
			{
				if ( MAPROTATION_MapHasLowestOrHighestLimit( i, ulLowestLimit, ulHighestLimit, bUseMaxLimit ))
					unusedEntries.push_back ( i );
			}
		}

		g_ulNextMapInList = unusedEntries[ M_Random ( unusedEntries.size() ) ];
	}
	else
	{
		g_ulNextMapInList = g_ulCurMapInList + 1;
		g_ulNextMapInList = ( g_ulNextMapInList % MAPROTATION_GetNumEntries( ));

		// [AK] Check if the next map in the list can be entered with the current number of players.
		if (( g_MapRotationEntries.size( ) > 1 ) && ( MAPROTATION_CanEnterMap( g_ulNextMapInList, ulPlayerCount ) == false ))
		{
			ULONG ulOldMapInList = g_ulNextMapInList;
			bool bNothingFound = false;

			do
			{
				// [AK] Cycle through the entire list until we find a map that can be entered.
				g_ulNextMapInList = (g_ulNextMapInList + 1) % g_MapRotationEntries.size( );

				// [AK] We went through the entire list and couldn't find a valid map.
				if ( g_ulNextMapInList == ulOldMapInList )
				{
					bNothingFound = true;
					break;
				}
			}
			while ( MAPROTATION_CanEnterMap( g_ulNextMapInList, ulPlayerCount ) == false );

			if ( bNothingFound )
			{
				bUseMaxLimit = MAPROTATION_GetLowestAndHighestLimits( ulPlayerCount, ulLowestLimit, ulHighestLimit );
				g_ulNextMapInList = ulOldMapInList;

				// [AK] Find the next map in the list with the lowest min player or highest max player limit.
				while ( MAPROTATION_MapHasLowestOrHighestLimit( g_ulNextMapInList, ulLowestLimit, ulHighestLimit, bUseMaxLimit ) == false )
				{
					g_ulNextMapInList = (g_ulNextMapInList + 1) % g_MapRotationEntries.size( );
					if ( g_ulNextMapInList == ulOldMapInList )
						break;
				}
			}
		}
	}

	// [AK] If we're the server, tell the clients what the next map is.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( updateClients ))
		SERVERCOMMANDS_SetNextMapPosition( );
}

//*****************************************************************************
//
level_info_t *MAPROTATION_GetNextMap( void )
{
	// [BB] If we don't want to use the rotation, there is no scheduled next map.
	if (( sv_maprotation == false ) || ( g_MapRotationEntries.empty( )))
		return NULL;

	return ( g_MapRotationEntries[g_ulNextMapInList].map );
}

//*****************************************************************************
//
level_info_t *MAPROTATION_GetMap( ULONG ulIdx )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return ( NULL );

	return ( g_MapRotationEntries[ulIdx].map );
}

//*****************************************************************************
//
ULONG MAPROTATION_GetPlayerLimits( ULONG ulIdx, bool bMaxPlayers )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return ( 0 );

	return ( bMaxPlayers ? g_MapRotationEntries[ulIdx].maxPlayers : g_MapRotationEntries[ulIdx].minPlayers );
}

//*****************************************************************************
//
void MAPROTATION_SetPositionToMap( const char *mapName, const bool setNextMap )
{
	for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
	{
		if ( stricmp( g_MapRotationEntries[i].map->mapname, mapName ) == 0 )
		{
			g_ulCurMapInList = i;
			g_MapRotationEntries[g_ulCurMapInList].isUsed = true;
			break;
		}
	}

	// [AK] Set the next map position to the current position, if desired.
	if ( setNextMap )
		MAPROTATION_SetNextPosition( g_ulCurMapInList, false );
}

//*****************************************************************************
//
bool MAPROTATION_IsMapInRotation( const char *pszMapName )
{
	for ( ULONG ulIdx = 0; ulIdx < g_MapRotationEntries.size( ); ulIdx++ )
	{
		if ( stricmp( g_MapRotationEntries[ulIdx].map->mapname, pszMapName ) == 0 )
			return true;
	}
	return false;
}

//*****************************************************************************
//
bool MAPROTATION_IsUsed( ULONG ulIdx )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return ( false );

	return ( g_MapRotationEntries[ulIdx].isUsed );
}

//*****************************************************************************
//
void MAPROTATION_SetUsed( ULONG ulIdx, bool bUsed )
{
	if ( ulIdx >= g_MapRotationEntries.size( ))
		return;

	g_MapRotationEntries[ulIdx].isUsed = bUsed;
}

//*****************************************************************************
//
void MAPROTATION_AddMap( FCommandLine &argv, bool bSilent, bool bInsert )
{
	int iPosition = bInsert ? atoi( argv[2] ) : 0;
	int iLimitArg = bInsert ? 3 : 2;

	// [AK] Get the minimum and maximum player limits if they've been included.
	ULONG ulMinPlayers = ( argv.argc( ) > iLimitArg ) ? atoi( argv[iLimitArg] ) : 0;
	ULONG ulMaxPlayers = ( argv.argc( ) > iLimitArg + 1 ) ? atoi( argv[iLimitArg + 1] ) : MAXPLAYERS;

	MAPROTATION_AddMap( argv[1], iPosition, ulMinPlayers, ulMaxPlayers, bSilent );
}

//*****************************************************************************
//
void MAPROTATION_AddMap( const char *pszMapName, int iPosition, ULONG ulMinPlayers, ULONG ulMaxPlayers, bool bSilent )
{
	// Find the map.
	level_info_t *pMap = FindLevelByName( pszMapName );
	if ( pMap == NULL )
	{
		Printf( "map %s doesn't exist.\n", pszMapName );
		return;
	}

	// [AK] Save the position we originally passed into this function.
	int iOriginalPosition = iPosition;

	MapRotationEntry newEntry;
	newEntry.map = pMap;
	newEntry.isUsed = false;

	// [AK] Add the minimum and maximum player limits the map will use.
	newEntry.minPlayers = clamp<ULONG>( ulMinPlayers, 0, MAXPLAYERS );
	newEntry.maxPlayers = clamp<ULONG>( ulMaxPlayers, 1, MAXPLAYERS );

	// [AK] The minimum limit should never be greater than the maximum limit.
	if ( newEntry.minPlayers > newEntry.maxPlayers )
		swapvalues( newEntry.minPlayers, newEntry.maxPlayers );

	// [Dusk] iPosition of 0 implies the end of the maplist.
	if (iPosition == 0) {
		// Add it to the queue.
		g_MapRotationEntries.push_back( newEntry );
		
		// [Dusk] note down the position for output
		iPosition = g_MapRotationEntries.end() - g_MapRotationEntries.begin();
	} else {
		// [Dusk] insert the map into a certain position
		std::vector<MapRotationEntry>::iterator itPosition = g_MapRotationEntries.begin() + iPosition - 1;

		// sanity check.
		if (itPosition < g_MapRotationEntries.begin () || itPosition > g_MapRotationEntries.end ()) {
			Printf ("Bad index specified!\n");
			return;
		}

		g_MapRotationEntries.insert( itPosition, 1, newEntry );
	}

	// [AK] Set the current entry in the map rotation to the current level, but
	// only set the next entry if it's the only one in the rotation.
	MAPROTATION_SetPositionToMap( level.mapname, g_MapRotationEntries.size( ) == 1 );

	// [AK] If there's more than one entry in the map rotation now, and the
	// current and next entries are the same, calculate a new next map.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( g_MapRotationEntries.size( ) > 1 ) && ( g_ulCurMapInList == g_ulNextMapInList ))
		MAPROTATION_CalcNextMap( true );

	if ( !bSilent )
	{
		FString message;
		message.Format( "%s (%s) added to map rotation list at position %d", pMap->mapname, pMap->LookupLevelName( ).GetChars( ), iPosition );

		if (( newEntry.minPlayers > 0 ) || ( newEntry.maxPlayers < MAXPLAYERS ))
		{
			message += " (";

			if ( newEntry.minPlayers > 0 )
				message.AppendFormat( "min = %lu", newEntry.minPlayers );

			if ( newEntry.maxPlayers < MAXPLAYERS )
			{
				if ( newEntry.minPlayers > 0 )
					message += ", ";

				message.AppendFormat( "max = %lu", newEntry.maxPlayers );
			}

			message += ')';
		}

		Printf( "%s.\n", message.GetChars( ));
	}

	// [AK] If we're the server, tell the clients to add the map on their end.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_AddToMapRotation( pMap->mapname, iOriginalPosition, newEntry.minPlayers, newEntry.maxPlayers );
}

//*****************************************************************************
// [Dusk] Removes a map from map rotation
void MAPROTATION_DelMap (const char *pszMapName, bool bSilent)
{
	// look up the map
	level_info_t *pMap = FindLevelByName (pszMapName);
	if (pMap == NULL)
	{
		Printf ("map %s doesn't exist.\n", pszMapName);
		return;
	}

	// search the map in the map rotation and throw it to trash
	std::vector<MapRotationEntry>::iterator iterator;
	bool gotcha = false;
	for (iterator = g_MapRotationEntries.begin (); iterator < g_MapRotationEntries.end (); iterator++)
	{
		level_info_t *entry = iterator->map;
		if (!stricmp(entry->mapname, pszMapName))
		{
			level_info_t *nextEntry = MAPROTATION_GetNextMap( );

			g_MapRotationEntries.erase (iterator);
			gotcha = true;

			// [AK] If the deleted map was the next entry, calculate a new one.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( g_MapRotationEntries.size( ) > 0 ) && ( entry == nextEntry ))
				MAPROTATION_CalcNextMap( true );

			break;
		}
	}

	if (gotcha)
	{
		if ( !bSilent )
			Printf ( "%s (%s) has been removed from map rotation list.\n", pMap->mapname, pMap->LookupLevelName( ).GetChars( ));

		// [AK] If we're the server, tell the clients to remove the map on their end.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DelFromMapRotation( pszMapName );
	}
	else
	{
		Printf ("Map %s is not in rotation.\n", pszMapName);
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( addmap )
{
	if ( argv.argc( ) > 1 )
		MAPROTATION_AddMap( argv, false );
	else
		Printf( "addmap <lumpname> [minplayers] [maxplayers]: Adds a map to the map rotation list.\n" );
}

CCMD( addmapsilent ) // Backwards API needed for server console, RCON.
{
	if ( argv.argc( ) > 1 )
		MAPROTATION_AddMap( argv, true );
	else
		Printf( "addmapsilent <lumpname> [minplayers] [maxplayers]: Silently adds a map to the map rotation list.\n" );
}

//*****************************************************************************
//
CCMD( maplist )
{
	if ( g_MapRotationEntries.size( ) == 0 )
		Printf( "The map rotation list is empty.\n" );
	else
	{
		const unsigned int playerCount = MAPROTATION_CountEligiblePlayers( );
		FString message;

		Printf( "Map rotation list: \n" );
		for ( ULONG ulIdx = 0; ulIdx < g_MapRotationEntries.size( ); ulIdx++ )
		{
			const bool canEnter = MAPROTATION_CanEnterMap( ulIdx, playerCount );
			message.Format( "%lu. ", ulIdx + 1 );

			// [AK] Highlight the current position in the map rotation in green, but only if we're actually playing on that map.
			if (( g_ulCurMapInList == ulIdx ) && ( stricmp( level.mapname, g_MapRotationEntries[g_ulCurMapInList].map->mapname ) == 0 ))
			{
				message.Insert( 0, canEnter ? TEXTCOLOR_GREEN : TEXTCOLOR_DARKGREEN );
				message += "(Current) ";
			}
			// [AK] Highlight the next position in the map rotation in blue.
			else if ( g_ulNextMapInList == ulIdx )
			{
				message.Insert( 0, canEnter ? TEXTCOLOR_LIGHTBLUE : TEXTCOLOR_BLUE );
				message += "(Next) ";
			}
			// [AK] Highlight maps that have already been played in red.
			else if ( g_MapRotationEntries[ulIdx].isUsed )
			{
				message.Insert( 0, canEnter ? TEXTCOLOR_RED : TEXTCOLOR_DARKRED );
				message += "(Used) ";
			}
			// [AK] Maps that can't be entered are displayed in dark grey.
			else if ( canEnter == false )
			{
				message.Insert( 0, TEXTCOLOR_DARKGRAY );
			}

			message.AppendFormat( "%s - %s", g_MapRotationEntries[ulIdx].map->mapname, g_MapRotationEntries[ulIdx].map->LookupLevelName( ).GetChars( ));

			// [AK] Also print the min and max player limits if they're different from the default values.
			if (( g_MapRotationEntries[ulIdx].minPlayers > 0 ) || ( g_MapRotationEntries[ulIdx].maxPlayers < MAXPLAYERS ))
			{
				message += " (";

				if ( g_MapRotationEntries[ulIdx].minPlayers > 0 )
					message.AppendFormat( "min = %lu", g_MapRotationEntries[ulIdx].minPlayers );

				if ( g_MapRotationEntries[ulIdx].maxPlayers < MAXPLAYERS )
				{
					if ( g_MapRotationEntries[ulIdx].minPlayers > 0 )
						message += ", ";

					message.AppendFormat( "max = %lu", g_MapRotationEntries[ulIdx].maxPlayers );
				}

				message += ')';
			}

			Printf( "%s\n", message.GetChars() );
		}
	}
}

//*****************************************************************************
//
CCMD( clearmaplist )
{
	// [AK] Don't let clients clear the map rotation list for themselves.
	if ( NETWORK_InClientMode( ))
		return;

	// Reset the map list.
	MAPROTATION_Construct( );

	Printf( "Map rotation list cleared.\n" );
}

// [Dusk] delmap
CCMD (delmap) {
	if (argv.argc() > 1)
		MAPROTATION_DelMap (argv[1], false);
	else
		Printf ("delmap <lumpname>: Removes a map from the map rotation list.\n");
}

CCMD (delmapsilent) {
	if (argv.argc() > 1)
		MAPROTATION_DelMap (argv[1], true);
	else
		Printf ("delmapsilent <lumpname>: Silently removes a map from the map rotation list.\n");
}

CCMD (delmap_idx) {
	if (argv.argc() <= 1)
	{
		Printf ("delmap_idx <idx>: Removes a map from the map rotation list based in index number.\nUse maplist to list the rotation with index numbers.\n");
		return;
	}

	unsigned int idx = static_cast<unsigned int> ( ( atoi(argv[1]) - 1 ) );
	if ( idx >= g_MapRotationEntries.size() )
	{
		Printf ("No such map!\n");
		return;
	}

	Printf ("%s (%s) has been removed from map rotation list.\n",	g_MapRotationEntries[idx].map->mapname, g_MapRotationEntries[idx].map->LookupLevelName().GetChars());
	g_MapRotationEntries.erase (g_MapRotationEntries.begin()+idx);
}

//*****************************************************************************
// [Dusk] insertmap
CCMD (insertmap) {
	if ( argv.argc( ) > 2 )
		MAPROTATION_AddMap( argv, false, true );
	else
	{
		Printf( "insertmap <lumpname> <position> [minplayers] [maxplayers]: Inserts a map to the map rotation list, after <position>.\n"
			"Use maplist to list the rotation with index numbers.\n" );
	}
}

CCMD (insertmapsilent) {
	if ( argv.argc( ) > 2 )
		MAPROTATION_AddMap( argv, true, true );
	else
	{
		Printf( "insertmapsilent <lumpname> <position> [minplayers] [maxplayers]: Silently inserts a map to the map rotation list, after <position>.\n"
			"Use maplist to list the rotation with index numbers.\n" );
	}
}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_maprotation, true, CVAR_ARCHIVE | CVAR_GAMEPLAYSETTING );
CVAR( Bool, sv_randommaprotation, false, CVAR_ARCHIVE | CVAR_GAMEPLAYSETTING );
