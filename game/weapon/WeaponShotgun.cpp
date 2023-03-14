#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"

const int SHOTGUN_MOD_AMMO = BIT(0);

class rvWeaponShotgun : public rvWeapon {
public:

	CLASS_PROTOTYPE( rvWeaponShotgun );

	rvWeaponShotgun ( void );

	virtual void			Spawn				( void );
	void					Save				( idSaveGame *savefile ) const;
	void					Restore				( idRestoreGame *savefile );
	void					PreSave				( void );
	void					PostSave			( void );

protected:
	int						hitscans;
	
	//Make shotgun melee
	void				Attack(void);
	float               range;
	rvClientEffectPtr	impactEffect;
	int					impactMaterial;

private:

	stateResult_t		State_Idle		( const stateParms_t& parms );
	stateResult_t		State_Fire		( const stateParms_t& parms );
	stateResult_t		State_Reload	( const stateParms_t& parms );
	
	CLASS_STATES_PROTOTYPE( rvWeaponShotgun );
};

CLASS_DECLARATION( rvWeapon, rvWeaponShotgun )
END_CLASS

/*
================
rvWeaponShotgun::rvWeaponShotgun
================
*/
rvWeaponShotgun::rvWeaponShotgun( void ) {
}

/*
================
rvWeaponShotgun::Spawn
================
*/
void rvWeaponShotgun::Spawn( void ) {
	hitscans   = spawnArgs.GetFloat( "hitscans" );
	
	SetState( "Raise", 0 );	

	range = spawnArgs.GetFloat("range", "100");
	impactMaterial = -1;
	impactEffect = NULL;
}

/*
================
rvWeaponShotgun::Save
================
*/
void rvWeaponShotgun::Save( idSaveGame *savefile ) const {
}

/*
================
rvWeaponShotgun::Restore
================
*/
void rvWeaponShotgun::Restore( idRestoreGame *savefile ) {
	hitscans   = spawnArgs.GetFloat( "hitscans" );
}

/*
================
rvWeaponShotgun::PreSave
================
*/
void rvWeaponShotgun::PreSave ( void ) {
}

/*
================
rvWeaponShotgun::PostSave
================
*/
void rvWeaponShotgun::PostSave ( void ) {
}


/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION( rvWeaponShotgun )
	STATE( "Idle",				rvWeaponShotgun::State_Idle)
	STATE( "Fire",				rvWeaponShotgun::State_Fire )
	STATE( "Reload",			rvWeaponShotgun::State_Reload )
END_CLASS_STATES

/*
================
rvWeaponShotgun::State_Idle
================
*/
stateResult_t rvWeaponShotgun::State_Idle( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			if ( !AmmoAvailable( ) ) {
				SetStatus( WP_OUTOFAMMO );
			} else {
				SetStatus( WP_READY );
			}
		
			PlayCycle( ANIMCHANNEL_ALL, "idle", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_WAIT );
		
		case STAGE_WAIT:			
			if ( wsfl.lowerWeapon ) {
				SetState( "Lower", 4 );
				return SRESULT_DONE;
			}		
			if ( !clipSize ) {
				if ( gameLocal.time > nextAttackTime && wsfl.attack && AmmoAvailable ( ) ) {
					SetState( "Fire", 0 );
					return SRESULT_DONE;
				}  
			} else {				
				if ( gameLocal.time > nextAttackTime && wsfl.attack && AmmoInClip ( ) ) {
					SetState( "Fire", 0 );
					return SRESULT_DONE;
				}  
				if ( wsfl.attack && AutoReload() && !AmmoInClip ( ) && AmmoAvailable () ) {
					SetState( "Reload", 4 );
					return SRESULT_DONE;			
				}
				if ( wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable()>AmmoInClip()) ) {
					SetState( "Reload", 4 );
					return SRESULT_DONE;			
				}				
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponShotgun::State_Fire
================
*/
stateResult_t rvWeaponShotgun::State_Fire( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			
			//Normal shotgun attack
			//Attack( false, hitscans, spread, 0, 1.0f );

			//melee shotgun
			Attack();
			nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier(PMOD_FIRERATE));
			PlayAnim( ANIMCHANNEL_ALL, "fire", 0 );	
			return SRESULT_STAGE( STAGE_WAIT );
	
		case STAGE_WAIT:
			if ( (!gameLocal.isMultiplayer && (wsfl.lowerWeapon || AnimDone( ANIMCHANNEL_ALL, 0 )) ) || AnimDone( ANIMCHANNEL_ALL, 0 ) ) {
				SetState( "Idle", 0 );
				return SRESULT_DONE;
			}									
			if ( wsfl.attack && gameLocal.time >= nextAttackTime && AmmoInClip() ) {
				SetState( "Fire", 0 );
				return SRESULT_DONE;
			}
			if ( clipSize ) {
				if ( (wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable()>AmmoInClip())) ) {
					SetState( "Reload", 4 );
					return SRESULT_DONE;			
				}				
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponShotgun::State_Reload
================
*/
stateResult_t rvWeaponShotgun::State_Reload ( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
		STAGE_RELOADSTARTWAIT,
		STAGE_RELOADLOOP,
		STAGE_RELOADLOOPWAIT,
		STAGE_RELOADDONE,
		STAGE_RELOADDONEWAIT
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			if ( wsfl.netReload ) {
				wsfl.netReload = false;
			} else {
				NetReload ( );
			}
			
			SetStatus ( WP_RELOAD );
			
			if ( mods & SHOTGUN_MOD_AMMO ) {				
				PlayAnim ( ANIMCHANNEL_ALL, "reload_clip", parms.blendFrames );
			} else {
				PlayAnim ( ANIMCHANNEL_ALL, "reload_start", parms.blendFrames );
				return SRESULT_STAGE ( STAGE_RELOADSTARTWAIT );
			}
			return SRESULT_STAGE ( STAGE_WAIT );
			
		case STAGE_WAIT:
			if ( AnimDone ( ANIMCHANNEL_ALL, 4 ) ) {
				AddToClip ( ClipSize() );
				SetState ( "Idle", 4 );
				return SRESULT_DONE;
			}
			if ( wsfl.lowerWeapon ) {
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
			
		case STAGE_RELOADSTARTWAIT:
			if ( AnimDone ( ANIMCHANNEL_ALL, 0 ) ) {
				return SRESULT_STAGE ( STAGE_RELOADLOOP );
			}
			if ( wsfl.lowerWeapon ) {
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
			
		case STAGE_RELOADLOOP:		
			if ( (wsfl.attack && AmmoInClip() ) || AmmoAvailable ( ) <= AmmoInClip ( ) || AmmoInClip() == ClipSize() ) {
				return SRESULT_STAGE ( STAGE_RELOADDONE );
			}
			PlayAnim ( ANIMCHANNEL_ALL, "reload_loop", 0 );
			return SRESULT_STAGE ( STAGE_RELOADLOOPWAIT );
			
		case STAGE_RELOADLOOPWAIT:
			if ( (wsfl.attack && AmmoInClip() ) || wsfl.netEndReload ) {
				return SRESULT_STAGE ( STAGE_RELOADDONE );
			}
			if ( wsfl.lowerWeapon ) {
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			if ( AnimDone ( ANIMCHANNEL_ALL, 0 ) ) {
				AddToClip( 1 );
				return SRESULT_STAGE ( STAGE_RELOADLOOP );
			}
			return SRESULT_WAIT;
		
		case STAGE_RELOADDONE:
			NetEndReload ( );
			PlayAnim ( ANIMCHANNEL_ALL, "reload_end", 0 );
			return SRESULT_STAGE ( STAGE_RELOADDONEWAIT );

		case STAGE_RELOADDONEWAIT:
			if ( wsfl.lowerWeapon ) {
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			if ( wsfl.attack && AmmoInClip ( ) && gameLocal.time > nextAttackTime ) {
				SetState ( "Fire", 0 );
				return SRESULT_DONE;
			}
			if ( AnimDone ( ANIMCHANNEL_ALL, 4 ) ) {
				SetState ( "Idle", 4 );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;	
}
			

void rvWeaponShotgun::Attack(void) {
	trace_t		tr;
	idEntity* ent;
	// Cast a ray out to the lock range
// RAVEN BEGIN
// ddynerman: multiple clip worlds
	gameLocal.TracePoint(owner, tr, playerViewOrigin,playerViewOrigin + playerViewAxis[0] * range, MASK_SHOT_RENDERMODEL, owner);
	// RAVEN END
	owner->WeaponFireFeedback(&weaponDef->dict);

	if (tr.fraction >= 1.0f) {
		if (impactEffect) {
			impactEffect->Stop();
			impactEffect = NULL;
		}
		impactMaterial = -1;
		return;
	}

	// Entity we hit?
	ent = gameLocal.entities[tr.c.entityNum];

	// If the impact material changed then stop the impact effect 
	if ((tr.c.materialType && tr.c.materialType->Index() != impactMaterial) ||
		(!tr.c.materialType && impactMaterial != -1)) {
		if (impactEffect) {
			impactEffect->Stop();
			impactEffect = NULL;
		}
		impactMaterial = -1;
	}

	// In singleplayer-- the gauntlet never effects marine AI
	if (!gameLocal.isMultiplayer) {
		idActor* actor_ent = 0;

		//ignore both the body and the head.
		if (ent->IsType(idActor::GetClassType())) {
			actor_ent = static_cast<idActor*>(ent);
		}
		else if (ent->IsType(idAFAttachment::GetClassType())) {
			actor_ent = static_cast<idActor*>(ent->GetBindMaster());
		}

		if (actor_ent && actor_ent->team == gameLocal.GetLocalPlayer()->team) {
			return;
		}
	}

	//multiplayer-- don't gauntlet dead stuff
	if (gameLocal.isMultiplayer) {
		idPlayer* player;
		if (ent->IsType(idPlayer::GetClassType())) {
			player = static_cast<idPlayer*>(ent);
			if (player->health <= 0) {
				return;
			}
		}

	}

	if (!impactEffect) {
		impactMaterial = tr.c.materialType ? tr.c.materialType->Index() : -1;
		impactEffect = gameLocal.PlayEffect(gameLocal.GetEffect(spawnArgs, "fx_impact", tr.c.materialType), tr.endpos, tr.c.normal.ToMat3(), true);
	}
	else {
		impactEffect->SetOrigin(tr.endpos);
		impactEffect->SetAxis(tr.c.normal.ToMat3());
	}
	// Do damage?
	if (gameLocal.time > nextAttackTime) {
		if (ent) {
			if (ent->fl.takedamage) {
				float dmgScale = 1.0f;
				dmgScale *= owner->PowerUpModifier(PMOD_MELEE_DAMAGE);
				ent->Damage(owner, owner, playerViewAxis[0], spawnArgs.GetString("def_damage"), dmgScale, 0);
			}
		}
		//nextAttackTime = gameLocal.time + fireRate;
	}
}

