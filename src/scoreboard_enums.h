//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2021-2022 Adam Kaminski
// Copyright (C) 2021-2022 Zandronum Development Team
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
// 3. Neither the name of the Zandronum Development Team nor the names of its
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
// Filename: scoreboard_enums.h
//
// Description: Contains any scoreboard-related enumerations.
//
//-----------------------------------------------------------------------------

#if ( !defined( __SCOREBOARD_ENUMS_H__ ) || defined( GENERATE_ENUM_STRINGS ))

#if ( !defined( GENERATE_ENUM_STRINGS ))
	#define __SCOREBOARD_ENUMS_H__
#endif

#include "EnumToString.h"

//*****************************************************************************
//
// How the contents inside of a column are aligned.
//
BEGIN_ENUM( COLUMNALIGN_e )
{
	// Aligns contents to the left of the column header.
	ENUM_ELEMENT( COLUMNALIGN_LEFT ),
	// Aligns contents to the center of the column header.
	ENUM_ELEMENT( COLUMNALIGN_CENTER ),
	// Aligns contents to the right of the column header.
	ENUM_ELEMENT( COLUMNALIGN_RIGHT ),
}
END_ENUM( COLUMNALIGN_e )

//*****************************************************************************
//
// Supported column data types.
//
BEGIN_ENUM( COLUMNDATA_e )
{
	ENUM_ELEMENT( COLUMNDATA_UNKNOWN ),
	// Treat the column as an integer.
	ENUM_ELEMENT( COLUMNDATA_INT ),
	// Treat the column as a boolean (1 = true, 0 = false).
	ENUM_ELEMENT( COLUMNDATA_BOOL ),
	// Treat the column as a float or fixed-point number.
	ENUM_ELEMENT( COLUMNDATA_FLOAT ),
	// Treat the column as text.
	ENUM_ELEMENT( COLUMNDATA_STRING ),
	// Treat the column as a box drawn in a flat color.
	ENUM_ELEMENT( COLUMNDATA_COLOR ),
	// Treat the column as a graphic or image.
	ENUM_ELEMENT( COLUMNDATA_TEXTURE ),
}
END_ENUM( COLUMNDATA_e )

//*****************************************************************************
//
// Native column types.
//
BEGIN_ENUM( COLUMNTYPE_e )
{
	ENUM_ELEMENT( COLUMNTYPE_UNKNOWN ),
	// The name this player is using.
	ENUM_ELEMENT( COLUMNTYPE_NAME ),
	// The player's index number.
	ENUM_ELEMENT( COLUMNTYPE_INDEX ),
	// How long has this player played in the current game?
	ENUM_ELEMENT( COLUMNTYPE_TIME ),
	// The player's ping measured in milliseconds.
	ENUM_ELEMENT( COLUMNTYPE_PING ),
	// The player's current frag count.
	ENUM_ELEMENT( COLUMNTYPE_FRAGS ),
	// The player's current point count.
	ENUM_ELEMENT( COLUMNTYPE_POINTS ),
	// The player's current win count.
	ENUM_ELEMENT( COLUMNTYPE_WINS ),
	// The player's current kill count.
	ENUM_ELEMENT( COLUMNTYPE_KILLS ),
	// How many times this player has died.
	ENUM_ELEMENT( COLUMNTYPE_DEATHS ),
	// The number of secrets this player has discovered.
	ENUM_ELEMENT( COLUMNTYPE_SECRETS ),
	// The number of lives this player still has.
	ENUM_ELEMENT( COLUMNTYPE_LIVES ),
	// How much damage the player has dealt, if ZADF_AWARD_DAMAGE_INSTEAD_KILLS is enabled.
	ENUM_ELEMENT( COLUMNTYPE_DAMAGE ),
	// The player's handicap value.
	ENUM_ELEMENT( COLUMNTYPE_HANDICAP ),
	// The player's position in the join queue.
	ENUM_ELEMENT( COLUMNTYPE_JOINQUEUE ),
	// What decision this player made for the current vote.
	ENUM_ELEMENT( COLUMNTYPE_VOTE ),
	// The current colour that this player is using.
	ENUM_ELEMENT( COLUMNTYPE_PLAYERCOLOR ),
	// The status of the player.
	ENUM_ELEMENT( COLUMNTYPE_STATUSICON ),
	// Whether or not a player is ready to go on the intermission screen.
	ENUM_ELEMENT( COLUMNTYPE_READYTOGOICON ),
	// The ScoreIcon of the player's class.
	ENUM_ELEMENT( COLUMNTYPE_SCOREICON ),
	// When a player is carrying a gamemode-related item (e.g. another team's item).
	ENUM_ELEMENT( COLUMNTYPE_ARTIFACTICON ),
	// The skill level of a bot.
	ENUM_ELEMENT( COLUMNTYPE_BOTSKILLICON ),
	// A custom column that's defined by the modder.
	ENUM_ELEMENT( COLUMNTYPE_CUSTOM ),
}
END_ENUM( COLUMNTYPE_e )

//*****************************************************************************
//
// Flags for the columns.
//
BEGIN_ENUM( COLUMNFLAG_e )
{
	// Orders players in reverse (i.e. least to greatest).
	ENUM_ELEMENT2( COLUMNFLAG_REVERSEORDER, 0x01 ),
	// This column only appears on the intermission screen.
	ENUM_ELEMENT2( COLUMNFLAG_INTERMISSIONONLY, 0x02 ),
	// This column won't appear on the intermission screen.
	ENUM_ELEMENT2( COLUMNFLAG_NOINTERMISSION, 0x04 ),
	// Don't draw the contents of this column for true spectators.
	ENUM_ELEMENT2( COLUMNFLAG_NOSPECTATORS, 0x08 ),
	// This column only appears in offline games.
	ENUM_ELEMENT2( COLUMNFLAG_OFFLINEONLY, 0x10 ),
	// This column only appears in online games.
	ENUM_ELEMENT2( COLUMNFLAG_ONLINEONLY, 0x20 ),
	// This column is only active in game modes that support teams.
	ENUM_ELEMENT2( COLUMNFLAG_REQUIRESTEAMS, 0x40 ),
	// This column is disabled in game modes that support teams.
	ENUM_ELEMENT2( COLUMNFLAG_FORBIDTEAMS, 0x80 ),
	// This column is only active in game modes where players have lives.
	ENUM_ELEMENT2( COLUMNFLAG_REQUIRESLIVES, 0x100 ),
	// This column is disabled in game modes that use lives.
	ENUM_ELEMENT2( COLUMNFLAG_FORBIDLIVES, 0x200 ),
	// This column is only active in game modes where a team's item is used.
	ENUM_ELEMENT2( COLUMNFLAG_REQUIRESTEAMITEMS, 0x400 ),
	// This column is disabled in game modes that use team items.
	ENUM_ELEMENT2( COLUMNFLAG_FORBIDTEAMITEMS, 0x800 ),
	// Don't show this column at the start of the game.
	ENUM_ELEMENT2( COLUMNFLAG_HIDDENBYDEFAULT, 0x1000 ),
	// Prevents this column's header from being shown.
	ENUM_ELEMENT2( COLUMNFLAG_DONTSHOWHEADER, 0x2000 ),
	// The column's width is always set to whatever's the shortest possible width.
	ENUM_ELEMENT2( COLUMNFLAG_ALWAYSUSESHORTESTWIDTH, 0x4000 ),
	// The column's CVar must be zero for the column to stay active.
	ENUM_ELEMENT2( COLUMNFLAG_CVARMUSTBEZERO, 0x8000 ),
	// If the column's empty (i.e. no contents inside it), then it's disabled.
	ENUM_ELEMENT2( COLUMNFLAG_DISABLEIFEMPTY, 0x10000 ),
	// This column's values aren't reset to default when the level changes (custom columns only).
	ENUM_ELEMENT2( COLUMNFLAG_DONTRESETONLEVELCHANGE, 0x20000 ),
}
END_ENUM( COLUMNFLAG_e )

//*****************************************************************************
//
// Flags for the scoreboard.
//
BEGIN_ENUM( SCOREBOARDFLAG_e )
{
	// Row text will be printed in the same color as the player's team.
	ENUM_ELEMENT2( SCOREBOARDFLAG_USETEAMTEXTCOLOR, 0x01 ),
	// The text color of the headers is automatically used to color the border lines.
	ENUM_ELEMENT2( SCOREBOARDFLAG_USEHEADERCOLORFORBORDERS, 0x02 ),
	// The borders are drawn using textures instead of lines.
	ENUM_ELEMENT2( SCOREBOARDFLAG_USETEXTUREFORBORDERS, 0x04 ),
	// Shows the gaps between columns on the row's background.
	ENUM_ELEMENT2( SCOREBOARDFLAG_SHOWGAPSINROWBACKGROUND, 0x08 ),
	// Don't draw any borders on the scoreboard.
	ENUM_ELEMENT2( SCOREBOARDFLAG_DONTDRAWBORDERS, 0x10 ),
	// Players aren't divided into their respective teams and appear on a single list.
	ENUM_ELEMENT2( SCOREBOARDFLAG_DONTSEPARATETEAMS, 0x20 ),
	// The local row background color is never used.
	ENUM_ELEMENT2( SCOREBOARDFLAG_DONTUSELOCALROWBACKGROUNDCOLOR, 0x40 ),
}
END_ENUM( SCOREBOARDFLAG_e )

//*****************************************************************************
//
// All column commands in SCORINFO.
//
BEGIN_ENUM( COLUMNCMD_e )
{
	// The text that gets drawn in a column's header.
	ENUM_ELEMENT( COLUMNCMD_DISPLAYNAME ),
	// A shorter or abbreviated version of the display name.
	ENUM_ELEMENT( COLUMNCMD_SHORTNAME ),
	// How the contents inside the column are aligned (left, center, or right).
	ENUM_ELEMENT( COLUMNCMD_ALIGNMENT ),
	// The size of the column (can be either the whole width or padding), in pixels.
	ENUM_ELEMENT( COLUMNCMD_SIZE ),
	// A list of game modes where this column is only active, if not empty.
	ENUM_ELEMENT( COLUMNCMD_GAMEMODE ),
	// The game types this column is only active in (i.e. cooperative, deathmatch, teamgame).
	ENUM_ELEMENT( COLUMNCMD_GAMETYPE ),
	// What players must earn for this column to be active (i.e. kills, frags, points, wins).
	ENUM_ELEMENT( COLUMNCMD_EARNTYPE ),
	// The CVar (integer or boolean) that decides if this column is active (if non-zero) or disabled.
	ENUM_ELEMENT( COLUMNCMD_CVAR ),
	// Limits how many digits (integer), decimals (float), or characters (boolean or string) are shown.
	ENUM_ELEMENT( COLUMNCMD_MAXLENGTH ),
	// A string of text placed in front of the value (for text-based columns).
	ENUM_ELEMENT( COLUMNCMD_PREFIX ),
	// A string of text placed behind the value (for text-based columns).
	ENUM_ELEMENT( COLUMNCMD_SUFFIX ),
	// The width of the clipping rectangle (for colors or textures), where anything outside of it isn't drawn.
	ENUM_ELEMENT( COLUMNCMD_CLIPRECTWIDTH ),
	// The height of the clipping rectangle (for colors or textures).
	ENUM_ELEMENT( COLUMNCMD_CLIPRECTHEIGHT ),
	// What gets drawn when a row's value is 1 (boolean columns only).
	ENUM_ELEMENT( COLUMNCMD_TRUETEXT ),
	// What gets drawn when a row's value is 0 (boolean columns only).
	ENUM_ELEMENT( COLUMNCMD_FALSETEXT ),
	// The default value of this column at the start of a new game (custom columns only).
	ENUM_ELEMENT( COLUMNCMD_DEFAULTVALUE ),
	// What sub-columns are inside the composite column and their order (composite columns only).
	ENUM_ELEMENT( COLUMNCMD_COLUMNS ),

	ENUM_ELEMENT( NUM_COLUMNCMDS )
}
END_ENUM( COLUMNCMD_e )

//*****************************************************************************
//
// All scoreboard commands in SCORINFO.
//
BEGIN_ENUM( SCOREBOARDCMD_e )
{
	// The font used to draw the column's header.
	ENUM_ELEMENT( SCOREBOARDCMD_HEADERFONT ),
	// The font used to draw the rows for each player.
	ENUM_ELEMENT( SCOREBOARDCMD_ROWFONT ),
	// The text color of the column's header.
	ENUM_ELEMENT( SCOREBOARDCMD_HEADERCOLOR ),
	// The text color used for the rows of all players.
	ENUM_ELEMENT( SCOREBOARDCMD_ROWCOLOR ),
	// The text color used for the row of the player being spied on.
	ENUM_ELEMENT( SCOREBOARDCMD_LOCALROWCOLOR ),
	// Similar to the local row color, but only while watching a demo.
	ENUM_ELEMENT( SCOREBOARDCMD_LOCALROWDEMOCOLOR ),
	// The opacity of the row's text for dead players.
	ENUM_ELEMENT( SCOREBOARDCMD_DEADPLAYERTEXTALPHA ),
	// The texture to use to draw the borders, if USETEXTUREFORBORDERS is enabled.
	ENUM_ELEMENT( SCOREBOARDCMD_BORDERTEXTURE ),
	// The "light" border line color, if USEHEADERCOLORFORBORDERS is disabled.
	ENUM_ELEMENT( SCOREBOARDCMD_LIGHTBORDERCOLOR ),
	// The "dark" border line color, if USEHEADERCOLORFORBORDERS is disabled.
	ENUM_ELEMENT( SCOREBOARDCMD_DARKBORDERCOLOR ),
	// The color of the background behind the entire scoreboard.
	ENUM_ELEMENT( SCOREBOARDCMD_BACKGROUNDCOLOR ),
	// The "light" row background color.
	ENUM_ELEMENT( SCOREBOARDCMD_LIGHTROWBACKGROUNDCOLOR ),
	// The "dark" row background color.
	ENUM_ELEMENT( SCOREBOARDCMD_DARKROWBACKGROUNDCOLOR ),
	// The color of the row background corresponding to the player being spied on.
	ENUM_ELEMENT( SCOREBOARDCMD_LOCALROWBACKGROUNDCOLOR ),
	// The opacity of the scoreboard's background.
	ENUM_ELEMENT( SCOREBOARDCMD_BACKGROUNDAMOUNT ),
	// The opacity of the row's background.
	ENUM_ELEMENT( SCOREBOARDCMD_ROWBACKGROUNDAMOUNT ),
	// The opacity of the row's background for dead players.
	ENUM_ELEMENT( SCOREBOARDCMD_DEADPLAYERROWBACKGROUNDAMOUNT ),
	// The spacing between the edges of the scoreboard's background and the text, in pixels.
	ENUM_ELEMENT( SCOREBOARDCMD_BACKGROUNDBORDERSIZE ),
	// The spacing between the column's header and the player rows, in pixels.
	ENUM_ELEMENT( SCOREBOARDCMD_GAPBETWEENHEADERANDROWS ),
	// The spacing between the columns, in pixels.
	ENUM_ELEMENT( SCOREBOARDCMD_GAPBETWEENCOLUMNS ),
	// The spacing between the player rows, in pixels.
	ENUM_ELEMENT( SCOREBOARDCMD_GAPBETWEENROWS ),
	// The height of the column headers.
	ENUM_ELEMENT( SCOREBOARDCMD_HEADERHEIGHT ),
	// The height of each player row.
	ENUM_ELEMENT( SCOREBOARDCMD_ROWHEIGHT ),
	// (Re)arranges columns on the scoreboard from left to right.
	ENUM_ELEMENT( SCOREBOARDCMD_COLUMNORDER ),
	// Adds a column to the end of the column order.
	ENUM_ELEMENT( SCOREBOARDCMD_ADDTOCOLUMNORDER ),
	// Determines how players are sorted on the scoreboard, from top to bottom.
	ENUM_ELEMENT( SCOREBOARDCMD_RANKORDER ),
	// Adds a column to the end of the rank order.
	ENUM_ELEMENT( SCOREBOARDCMD_ADDTORANKORDER ),

	ENUM_ELEMENT( NUM_SCOREBOARDCMDS )
}
END_ENUM( SCOREBOARDCMD_e )

#endif // ( !defined( __SCOREBOARD_ENUMS_H__ ) || defined( GENERATE_ENUM_STRINGS ))
