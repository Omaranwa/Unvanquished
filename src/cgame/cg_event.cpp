/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2009 Darklegion Development

This file is part of Daemon.

Daemon is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// cg_event.c -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"

/*
=============
CG_Obituary
=============
*/

static void CG_Obituary(entityState_t *ent)
{
	int targetNum, attackerNum, assistantNum, mod;
	clientInfo_t *target, *attacker = nullptr, *assistant = nullptr;

	targetNum = ent->otherEntityNum;
	attackerNum = ent->otherEntityNum2;
	assistantNum = ent->otherEntityNum3;
	mod = ent->eventParm;

	if (targetNum < 0 || targetNum > MAX_CLIENTS) {
		Log::Warn("CG_Obituary: invalid target number %i\n", targetNum);
		return;
	}

	target = cgs.clientinfo + targetNum;
	
	if (attackerNum >= 0 && attackerNum < MAX_CLIENTS)
		attacker = cgs.clientinfo + attackerNum;

	if (assistantNum >= 0 && assistantNum < MAX_CLIENTS)
		assistant = cgs.clientinfo + assistantNum;

	Rocket_GameLog_Obituary(target, attacker, assistant, mod);
}

//==========================================================================

/*
================
CG_PainEvent

Also called by playerstate transition
================
*/
void CG_PainEvent( centity_t *cent, int health )
{
	const char *snd;

	// don't do more than two pain sounds a second
	if ( cg.time - cent->pe.painTime < 500 )
	{
		return;
	}

	if ( health < 25 )
	{
		snd = "*pain25_1.wav";
	}
	else if ( health < 50 )
	{
		snd = "*pain50_1.wav";
	}
	else if ( health < 75 )
	{
		snd = "*pain75_1.wav";
	}
	else
	{
		snd = "*pain100_1.wav";
	}

	trap_S_StartSound( nullptr, cent->currentState.number, soundChannel_t::CHAN_VOICE,
	                   CG_CustomSound( cent->currentState.number, snd ) );

	// save pain time for programitic twitch animation
	cent->pe.painTime = cg.time;
	cent->pe.painDirection ^= 1;
}

/*
=========================
CG_OnPlayerWeaponChange

Called on weapon change
=========================
*/
void CG_OnPlayerWeaponChange()
{
	playerState_t *ps = &cg.snap->ps;

	// Change the HUD to match the weapon. Close the old hud first
	Rocket_ShowHud( ps->weapon );

	// Rebuild weapon lists if UI is in focus.
	if ( trap_Key_GetCatcher() == KEYCATCH_UI && ps->persistant[ PERS_TEAM ] == TEAM_HUMANS )
	{
		CG_Rocket_BuildArmourySellList( "default" );
		CG_Rocket_BuildArmouryBuyList( "default" );
	}

	cg.weaponOffsetsFilter.Reset( );

	cg.predictedPlayerEntity.pe.weapon.animationNumber = -1; //force weapon lerpframe recalculation
}

/*
=========================
CG_OnPlayerUpgradeChange

Called on upgrade change
=========================
*/

void CG_OnPlayerUpgradeChange()
{
	playerState_t *ps = &cg.snap->ps;

	// Rebuild weapon lists if UI is in focus.
	if ( trap_Key_GetCatcher() == KEYCATCH_UI && ps->persistant[ PERS_TEAM ] == TEAM_HUMANS )
	{
		CG_Rocket_BuildArmourySellList( "default" );
		CG_Rocket_BuildArmouryBuyList( "default" );
	}
}

/*
=========================
CG_OnMapRestart

Called whenever the map is restarted
via map_restart
=========================
*/
void CG_OnMapRestart()
{
	// if scoreboard is showing, hide it
	CG_HideScores_f();

	// hide any other menus
	Rocket_DocumentAction( "", "blurall" );
}

/*
==============
CG_Momentum

Notify player of generated momentum
==============
*/
void CG_Momentum( entityState_t *es )
{
	float                  momentum;
	bool               negative;

	negative   = es->groundEntityNum;
	momentum = ( negative ? -es->otherEntityNum2 : es->otherEntityNum2 ) / 10.0f;

	cg.momentumGained     = momentum;
	cg.momentumGainedTime = cg.time;
}

/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
void CG_EntityEvent( centity_t *cent, vec3_t position )
{
	entityState_t *es;
	int           event;
	vec3_t        dir;
	const char    *s;
	int           clientNum;
	clientInfo_t  *ci;
	int           steptime;

	if ( cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
	{
		steptime = 200;
	}
	else
	{
		steptime = BG_Class( cg.snap->ps.stats[ STAT_CLASS ] )->steptime;
	}

	es = &cent->currentState;
	event = es->event & ~EV_EVENT_BITS;

	if ( cg_debugEvents.integer )
	{
		Log::Debug( "ent:%3i  event:%3i %s", es->number, event,
		           BG_EventName( event ) );
	}

	if ( !event )
	{
		return;
	}

	clientNum = es->clientNum;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS )
	{
		clientNum = 0;
	}

	ci = &cgs.clientinfo[ clientNum ];

	switch ( event )
	{
		case EV_FOOTSTEP:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				if ( ci->footsteps == FOOTSTEP_CUSTOM )
				{
					trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
					                   ci->customFootsteps[ rand() & 3 ] );
				}
				else
				{
					trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
					                   cgs.media.footsteps[ ci->footsteps ][ rand() & 3 ] );
				}
			}

			break;

		case EV_FOOTSTEP_METAL:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				if ( ci->footsteps == FOOTSTEP_CUSTOM )
				{
					trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
					                   ci->customMetalFootsteps[ rand() & 3 ] );
				}
				else
				{
					trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
					                   cgs.media.footsteps[ FOOTSTEP_METAL ][ rand() & 3 ] );
				}
			}

			break;

		case EV_FOOTSTEP_SQUELCH:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
				                   cgs.media.footsteps[ FOOTSTEP_FLESH ][ rand() & 3 ] );
			}

			break;

		case EV_FOOTSPLASH:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
				                   cgs.media.footsteps[ FOOTSTEP_SPLASH ][ rand() & 3 ] );
			}

			break;

		case EV_FOOTWADE:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
				                   cgs.media.footsteps[ FOOTSTEP_SPLASH ][ rand() & 3 ] );
			}

			break;

		case EV_SWIM:
			if ( cg_footsteps.integer && ci->footsteps != FOOTSTEP_NONE )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY,
				                   cgs.media.footsteps[ FOOTSTEP_SPLASH ][ rand() & 3 ] );
			}

			break;

		case EV_FALL_SHORT:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.landSound );

			if ( clientNum == cg.predictedPlayerState.clientNum )
			{
				// smooth landing z changes
				cg.landChange = -8;
				cg.landTime = cg.time;
			}

			break;

		case EV_FALL_MEDIUM:
			// use a general pain sound
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, "*pain100_1.wav" ) );

			if ( clientNum == cg.predictedPlayerState.clientNum )
			{
				// smooth landing z changes
				cg.landChange = -16;
				cg.landTime = cg.time;
			}

			break;

		case EV_FALL_FAR:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, CG_CustomSound( es->number, "*fall1.wav" ) );
			cent->pe.painTime = cg.time; // don't play a pain sound right after this

			if ( clientNum == cg.predictedPlayerState.clientNum )
			{
				// smooth landing z changes
				cg.landChange = -24;
				cg.landTime = cg.time;
			}

			break;

		case EV_FALLING:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, CG_CustomSound( es->number, "*falling1.wav" ) );
			break;

		case EV_STEP_4:
		case EV_STEP_8:
		case EV_STEP_12:
		case EV_STEP_16: // smooth out step up transitions
		case EV_STEPDN_4:
		case EV_STEPDN_8:
		case EV_STEPDN_12:
		case EV_STEPDN_16: // smooth out step down transitions
			{
				float oldStep;
				int   delta;
				int   step;

				if ( clientNum != cg.predictedPlayerState.clientNum )
				{
					break;
				}

				// if we are interpolating, we don't need to smooth steps
				if ( cg.demoPlayback || ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ||
				     cg_nopredict.integer || cg.pmoveParams.synchronous )
				{
					break;
				}

				// check for stepping up before a previous step is completed
				delta = cg.time - cg.stepTime;

				if ( delta < steptime )
				{
					oldStep = cg.stepChange * ( steptime - delta ) / steptime;
				}
				else
				{
					oldStep = 0;
				}

				// add this amount
				if ( event >= EV_STEPDN_4 )
				{
					step = 4 * ( event - EV_STEPDN_4 + 1 );
					cg.stepChange = oldStep - step;
				}
				else
				{
					step = 4 * ( event - EV_STEP_4 + 1 );
					cg.stepChange = oldStep + step;
				}

				if ( cg.stepChange > MAX_STEP_CHANGE )
				{
					cg.stepChange = MAX_STEP_CHANGE;
				}
				else if ( cg.stepChange < -MAX_STEP_CHANGE )
				{
					cg.stepChange = -MAX_STEP_CHANGE;
				}

				cg.stepTime = cg.time;
				break;
			}

		case EV_JUMP:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, "*jump1.wav" ) );

			if ( BG_ClassHasAbility( cg.predictedPlayerState.stats[ STAT_CLASS ], SCA_WALLJUMPER ) )
			{
				vec3_t surfNormal, refNormal = { 0.0f, 0.0f, 1.0f };
				vec3_t rotAxis;

				if ( clientNum != cg.predictedPlayerState.clientNum )
				{
					break;
				}

				//set surfNormal
				VectorCopy( cg.predictedPlayerState.grapplePoint, surfNormal );

				//if we are moving from one surface to another smooth the transition
				if ( !VectorCompare( surfNormal, cg.lastNormal ) && surfNormal[ 2 ] != 1.0f )
				{
					CrossProduct( refNormal, surfNormal, rotAxis );
					VectorNormalize( rotAxis );

					//add the op
					CG_addSmoothOp( rotAxis, 15.0f, 1.0f );
				}

				//copy the current normal to the lastNormal
				VectorCopy( surfNormal, cg.lastNormal );
			}

			break;

		case EV_LEV4_TRAMPLE_PREPARE:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, cgs.media.alienL4ChargePrepare );
			break;

		case EV_LEV4_TRAMPLE_START:
			//FIXME: stop cgs.media.alienL4ChargePrepare playing here
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, cgs.media.alienL4ChargeStart );
			break;

		case EV_TAUNT:
			if ( !cg_noTaunt.integer )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, "*taunt.wav" ) );
			}

			break;

		case EV_WATER_TOUCH:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrInSound );
			break;

		case EV_WATER_LEAVE:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrOutSound );
			break;

		case EV_WATER_UNDER:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrUnSound );
			break;

		case EV_WATER_CLEAR:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, CG_CustomSound( es->number, "*gasp.wav" ) );
			break;

		case EV_JETPACK_ENABLE:
      cent->jetpackAnim = JANIM_SLIDEOUT;
			break;

		case EV_JETPACK_DISABLE:
      cent->jetpackAnim = JANIM_SLIDEIN;
			break;

		case EV_JETPACK_IGNITE:
			// TODO: Play jetpack ignite gfx/sfx
			break;

		case EV_JETPACK_START:
			// TODO: Start jetpack thrust gfx/sfx
			break;

		case EV_JETPACK_STOP:
			// TODO: Stop jetpack thrust gfx/sfx
			break;

		case EV_NOAMMO:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_WEAPON, cgs.media.weaponEmptyClick );
			break;

		case EV_CHANGE_WEAPON:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.selectSound );
			break;

		case EV_FIRE_WEAPON:
			CG_HandleFireWeapon( cent, WPM_PRIMARY );
			break;

		case EV_FIRE_WEAPON2:
			CG_HandleFireWeapon( cent, WPM_SECONDARY );
			break;

		case EV_FIRE_WEAPON3:
			CG_HandleFireWeapon( cent, WPM_TERTIARY );
			break;

		case EV_WEAPON_RELOAD:
			if ( cg_weapons[ es->eventParm ].wim[ WPM_PRIMARY ].reloadSound )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_WEAPON, cg_weapons[ es->eventParm ].wim[ WPM_PRIMARY ].reloadSound );
			}
			break;

		case EV_PLAYER_TELEPORT_IN:
			//deprecated
			break;

		case EV_PLAYER_TELEPORT_OUT:
			CG_PlayerDisconnect( position );
			break;

		case EV_BUILD_CONSTRUCT:
			break;

		case EV_BUILD_DESTROY:
			break;

		case EV_AMMO_REFILL:
		case EV_CLIPS_REFILL:
		case EV_FUEL_REFILL:
			// TODO: Add different sounds for EV_AMMO_REFILL, EV_CLIPS_REFILL, EV_FUEL_REFILL
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.repeaterUseSound );
			break;

		case EV_GRENADE_BOUNCE:
			if ( rand() & 1 )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.hardBounceSound1 );
			}
			else
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.hardBounceSound2 );
			}
			break;

		case EV_WEAPON_HIT_ENTITY:
			CG_HandleWeaponHitEntity( es, position );
			break;

		case EV_WEAPON_HIT_ENVIRONMENT:
			CG_HandleWeaponHitWall( es, position );
			break;

		case EV_MISSILE_HIT_ENTITY:
			CG_HandleMissileHitEntity( es, position );
			break;

		// currently there is no support for metal sounds
		case EV_MISSILE_HIT_ENVIRONMENT:
		case EV_MISSILE_HIT_METAL:
			CG_HandleMissileHitWall( es, position );
			break;

		case EV_SHOTGUN:
			CG_HandleFireShotgun( es );
			break;

		case EV_HUMAN_BUILDABLE_DYING:
			CG_HumanBuildableDying( (buildable_t) es->modelindex, position );
			break;

		case EV_HUMAN_BUILDABLE_EXPLOSION:
			ByteToDir( es->eventParm, dir );
			CG_HumanBuildableExplosion( (buildable_t) es->modelindex, position, dir );
			break;

		case EV_ALIEN_BUILDABLE_EXPLOSION:
			ByteToDir( es->eventParm, dir );
			CG_AlienBuildableExplosion( position, dir );
			break;

		case EV_TESLATRAIL:
			{
				centity_t *source = &cg_entities[ es->generic1 ];
				centity_t *target = &cg_entities[ es->clientNum ];

				if ( !CG_IsTrailSystemValid( &source->muzzleTS ) )
				{
					source->muzzleTS = CG_SpawnNewTrailSystem( cgs.media.teslaZapTS );

					if ( CG_IsTrailSystemValid( &source->muzzleTS ) )
					{
						CG_SetAttachmentCent( &source->muzzleTS->frontAttachment, source );
						CG_SetAttachmentCent( &source->muzzleTS->backAttachment, target );
						CG_AttachToCent( &source->muzzleTS->frontAttachment );
						CG_AttachToCent( &source->muzzleTS->backAttachment );

						source->muzzleTSDeathTime = cg.time + cg_teslaTrailTime.integer;
					}
				}
			}
			break;

		case EV_GENERAL_SOUND:
			if ( cgs.gameSounds[ es->eventParm ] )
			{
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, cgs.gameSounds[ es->eventParm ] );
			}
			else
			{
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );
				trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, s ) );
			}

			break;

		case EV_GLOBAL_SOUND: // play from the player's head so it never diminishes
			if ( cgs.gameSounds[ es->eventParm ] )
			{
				trap_S_StartSound( nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, cgs.gameSounds[ es->eventParm ] );
			}
			else
			{
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );
				trap_S_StartSound( nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, CG_CustomSound( es->number, s ) );
			}

			break;

		case EV_PAIN:
			// local player sounds are triggered in CG_CheckLocalSounds,
			// so ignore events on the player
			if ( cent->currentState.number != cg.snap->ps.clientNum )
			{
				CG_PainEvent( cent, es->eventParm );
			}

			break;

		case EV_DEATH1:
		case EV_DEATH2:
		case EV_DEATH3:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE,
			                   CG_CustomSound( es->number, va( "*death%i.wav", event - EV_DEATH1 + 1 ) ) );
			break;

		case EV_OBITUARY:
			CG_Obituary( es );
			break;

		case EV_GIB_PLAYER:
			// no gibbing
			break;

		case EV_STOPLOOPINGSOUND:
			trap_S_StopLoopingSound( es->number );
			es->loopSound = 0;
			break;

		case EV_BUILD_DELAY:
			if ( clientNum == cg.predictedPlayerState.clientNum )
			{
				trap_S_StartLocalSound( cgs.media.buildableRepairedSound, soundChannel_t::CHAN_LOCAL_SOUND );
				cg.lastBuildAttempt = cg.time;
			}

			break;

		case EV_BUILD_REPAIR:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.buildableRepairSound );
			break;

		case EV_BUILD_REPAIRED:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.buildableRepairedSound );
			break;

		case EV_OVERMIND_ATTACK_1:
		case EV_OVERMIND_ATTACK_2:
			if ( cg.predictedPlayerState.persistant[ PERS_TEAM ] == TEAM_ALIENS )
			{
				trap_S_StartLocalSound( cgs.media.alienOvermindAttack, soundChannel_t::CHAN_ANNOUNCER );
				CG_CenterPrint( va( "^%c%s", "31"[ event - EV_OVERMIND_ATTACK_1 ], _( "The Overmind is under attack!" ) ), 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_OVERMIND_DYING:
			if ( cg.predictedPlayerState.persistant[ PERS_TEAM ] == TEAM_ALIENS )
			{
				trap_S_StartLocalSound( cgs.media.alienOvermindDying, soundChannel_t::CHAN_ANNOUNCER );
				CG_CenterPrint( _( "^1The Overmind is dying!" ), 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_REACTOR_ATTACK_1:
		case EV_REACTOR_ATTACK_2:
			if ( cg.predictedPlayerState.persistant[ PERS_TEAM ] == TEAM_HUMANS )
			{
				CG_CenterPrint( va( "^%c%s", "31"[ event - EV_REACTOR_ATTACK_1 ], _( "The reactor is under attack!" ) ), 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_REACTOR_DYING:
			if ( cg.predictedPlayerState.persistant[ PERS_TEAM ] == TEAM_HUMANS )
			{
				CG_CenterPrint( _( "^1The reactor is about to explode!" ), 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_WARN_ATTACK:
			// if eventParm is non-zero, this is for humans and there's a nearby reactor or repeater, otherwise it's for aliens
			if ( es->eventParm >= MAX_CLIENTS && es->eventParm < MAX_GENTITIES )
			{
				const char *location;
				bool    base = cg_entities[ es->eventParm ].currentState.modelindex == BA_H_REACTOR;
				centity_t  *locent = CG_GetLocation( cg_entities[ es->eventParm ].currentState.origin );

				CG_CenterPrint( base ? _( "Our base is under attack!" ) : _( "A forward base is under attack!" ), 200, GIANTCHAR_WIDTH * 4 );

				if ( locent )
				{
					location = CG_ConfigString( CS_LOCATIONS + locent->currentState.generic1 );
				}
				else
				{
					location = CG_ConfigString( CS_LOCATIONS );
				}

				if ( location && *location )
				{
					Log::Notice( _( "%s Under attack – %s" ), base ? "[reactor]" : "[repeater]", location );
				}
				else
				{
					Log::Notice( _( "%s Under attack" ), base ? "[reactor]" : "[repeater]" );
				}
			}
			else // this is for aliens, and the overmind is in range
			{
				CG_CenterPrint( _( "Our base is under attack!" ), 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_MGTURRET_SPINUP:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.turretSpinupSound );
			break;

		case EV_OVERMIND_SPAWNS:
			if ( cg.predictedPlayerState.persistant[ PERS_TEAM ] == TEAM_ALIENS )
			{
				trap_S_StartLocalSound( cgs.media.alienOvermindSpawns, soundChannel_t::CHAN_ANNOUNCER );
				CG_CenterPrint( "The Overmind needs spawns!", 200, GIANTCHAR_WIDTH * 4 );
			}

			break;

		case EV_ALIEN_EVOLVE:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.alienEvolveSound );
			{
				particleSystem_t *ps = CG_SpawnNewParticleSystem( cgs.media.alienEvolvePS );

				if ( CG_IsParticleSystemValid( &ps ) )
				{
					CG_SetAttachmentCent( &ps->attachment, cent );
					CG_AttachToCent( &ps->attachment );
				}
			}

			if ( es->number == cg.clientNum )
			{
				CG_ResetPainBlend();
				cg.spawnTime = cg.time;
			}

			break;

		case EV_ALIEN_EVOLVE_FAILED:
			if ( clientNum == cg.predictedPlayerState.clientNum )
			{
				//FIXME: change to "negative" sound
				trap_S_StartLocalSound( cgs.media.buildableRepairedSound, soundChannel_t::CHAN_LOCAL_SOUND );
				cg.lastEvolveAttempt = cg.time;
			}

			break;

		case EV_ALIEN_ACIDTUBE:
			{
				particleSystem_t *ps = CG_SpawnNewParticleSystem( cgs.media.alienAcidTubePS );

				if ( CG_IsParticleSystemValid( &ps ) )
				{
					CG_SetAttachmentCent( &ps->attachment, cent );
					ByteToDir( es->eventParm, dir );
					CG_SetParticleSystemNormal( ps, dir );
					CG_AttachToCent( &ps->attachment );
				}
			}
			break;

		case EV_ALIEN_BOOSTER:
			{
				particleSystem_t *ps = CG_SpawnNewParticleSystem( cgs.media.alienBoosterPS );

				if ( CG_IsParticleSystemValid( &ps ) )
				{
					CG_SetAttachmentCent( &ps->attachment, cent );
					ByteToDir( es->eventParm, dir );
					CG_SetParticleSystemNormal( ps, dir );
					CG_AttachToCent( &ps->attachment );
				}
			}
			break;

		case EV_MEDKIT_USED:
			trap_S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.medkitUseSound );
			break;

		case EV_PLAYER_RESPAWN:
			if ( es->number == cg.clientNum )
			{
				cg.spawnTime = cg.time;
			}

			break;

		case EV_HIT:
			cg.hitTime = cg.time;
			break;

		case EV_MOMENTUM:
			CG_Momentum( es );
			break;

		default:
			Com_Error(errorParm_t::ERR_DROP,  "Unknown event: %i", event );
	}
}

/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents( centity_t *cent )
{
	entity_event_t event;
	entity_event_t oldEvent = EV_NONE;

	// check for event-only entities
	if ( cent->currentState.eType > entityType_t::ET_EVENTS )
	{
		event = Util::enum_cast<entity_event_t>( Util::ordinal(cent->currentState.eType) - Util::ordinal(entityType_t::ET_EVENTS) );

		if ( cent->previousEvent )
		{
			return; // already fired
		}

		cent->previousEvent = 1;

		cent->currentState.event = Util::ordinal(cent->currentState.eType) - Util::ordinal(entityType_t::ET_EVENTS);

		// Move the pointer to the entity that the
		// event was originally attached to
		if ( cent->currentState.eFlags & EF_PLAYER_EVENT )
		{
			cent = &cg_entities[ cent->currentState.otherEntityNum ];
			oldEvent = (entity_event_t) cent->currentState.event;
			cent->currentState.event = event;
		}
	}
	else
	{
		// check for events riding with another entity
		if ( cent->currentState.event == cent->previousEvent )
		{
			return;
		}

		cent->previousEvent = cent->currentState.event;

		if ( ( cent->currentState.event & ~EV_EVENT_BITS ) == 0 )
		{
			return;
		}
	}

	// calculate the position at exactly the frame time
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
	CG_SetEntitySoundPosition( cent );

	CG_EntityEvent( cent, cent->lerpOrigin );

	// If this was a reattached spilled event, restore the original event
	if ( oldEvent != EV_NONE )
	{
		cent->currentState.event = oldEvent;
	}
}
