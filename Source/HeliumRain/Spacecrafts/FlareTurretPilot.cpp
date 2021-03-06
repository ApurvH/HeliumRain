
#include "HeliumRain/Spacecrafts/FlareTurretPilot.h"
#include "../Flare.h"

#include "HeliumRain/Spacecrafts/FlarePilotHelper.h"
#include "HeliumRain/Spacecrafts/FlareTurret.h"
#include "HeliumRain/Spacecrafts/FlareRCS.h"
#include "HeliumRain/Spacecrafts/FlareSpacecraft.h"
#include "HeliumRain/Spacecrafts/FlareSpacecraftComponent.h"

#include "../Player/FlarePlayerController.h"
#include "../Game/FlareGame.h"
#include "../Game/AI/FlareCompanyAI.h"

DECLARE_CYCLE_STAT(TEXT("FlareTurretPilot Tick"), STAT_FlareTurretPilot_Tick, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareTurretPilot Tick Target"), STAT_FlareTurretPilot_Target, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareTurretPilot Tick Intersect"), STAT_FlareTurretPilot_Intersect, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareTurretPilot Tick Intersect Gun"), STAT_FlareTurretPilot_Intersect_Gun, STATGROUP_Flare);

DECLARE_CYCLE_STAT(TEXT("FlareTurretPilot GetNearestHostileShip"), STAT_FlareTurretPilot_GetNearestHostileShip, STATGROUP_Flare);


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareTurretPilot::UFlareTurretPilot(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TargetSelectionReactionTime = FMath::FRandRange(1.0, 1.5);
	TimeUntilNextTargetSelectionReaction = 0;

	FireReactionTime = FMath::FRandRange(0.1, 0.2);
	TimeUntilFireReaction = 0;
}


/*----------------------------------------------------
	Gameplay events
----------------------------------------------------*/

void UFlareTurretPilot::Initialize(const FFlareTurretPilotSave* Data, UFlareCompany* Company, UFlareTurret* OwnerTurret)
{
	// Main data
	Turret = OwnerTurret;
	PlayerCompany = Company;
	WantFire = false;

	// Setup properties
	if (Data)
	{
		TurretPilotData = *Data;
	}
}

void UFlareTurretPilot::PlayerSetAim(FVector AimDirection, float AimDistance)
{
	ManualAimDirection = AimDirection;
	ManualAimDistance = AimDistance;
}

void UFlareTurretPilot::PlayerStartFire()
{
	if (Turret->IsCloseToAim())
	{
		WantFire = true;
	}
}

void UFlareTurretPilot::PlayerStopFire()
{
	WantFire = false;
}

void UFlareTurretPilot::ClearInvalidTarget(PilotHelper::PilotTarget InvalidTarget)
{
	if(PilotTarget == InvalidTarget)
	{
		PilotTarget.Clear();
	}
}

void UFlareTurretPilot::TickPilot(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_FlareTurretPilot_Tick);

	TimeUntilNextTargetSelectionReaction -= DeltaSeconds;
	TimeUntilFireReaction -= DeltaSeconds;
	TimeUntilNextComponentSwitch-= DeltaSeconds;

	// Is the player manually controlling this ? If not, pick a target
	bool IsManuallyFiring = Turret->GetSpacecraft()->GetWeaponsSystem()->GetActiveWeaponType() == EFlareWeaponGroupType::WG_TURRET;
	if (!IsManuallyFiring)
	{
		ProcessTurretTargetSelection();
		AimAxis = FVector::ZeroVector;
	}

	// Fire director
	if (IsManuallyFiring && !Turret->IsIgnoreManualAim())
	{
		AimAxis = ManualAimDirection;

		// Try getting a target
		AActor* HitTarget = NULL;
		Turret->IsSafeToFire(0, HitTarget);
		if (!HitTarget || HitTarget == Turret->GetSpacecraft())
		{
			HitTarget = Turret->GetSpacecraft()->GetCurrentTarget().GetActor();
		}

		// Aim the turret toward the target or a distant point
		if (HitTarget && Cast<UPrimitiveComponent>(HitTarget->GetRootComponent()))
		{
			FVector Location = HitTarget->GetActorLocation();
			FVector Velocity = Cast<UPrimitiveComponent>(HitTarget->GetRootComponent())->GetPhysicsLinearVelocity() / 100;
			Turret->SetTarget(Location, Velocity);
		}
		else
		{
			FVector FireTargetLocation = Turret->GetMuzzleLocation(0) + ManualAimDirection * 100000;
			Turret->SetTarget(FireTargetLocation, FVector::ZeroVector);
		}
	}

	// Auto pilot
	else if (PilotTarget.IsValid())
	{
		if(PilotTarget.SpacecraftTarget)
		{
			if (TimeUntilNextComponentSwitch <= 0)
			{
				//FLOGV("%s Switch because of timeout", *Turret->GetReadableName());
				PilotTargetShipComponent = NULL;
			}
			else if (PilotTargetShipComponent)
			{
				if (!PilotTarget.Is(PilotTargetShipComponent->GetSpacecraft()))
				{
					//FLOGV("%s Switch because the component %s is not in the target ship", *Turret->GetReadableName(), *PilotTargetShipComponent->GetReadableName());
					PilotTargetShipComponent = NULL;
				}
				else if (PilotTargetShipComponent->GetUsableRatio() <=0)
				{
					//FLOGV("%s Switch because the component %s is destroyed", *Turret->GetReadableName(), *PilotTargetShipComponent->GetReadableName());
					PilotTargetShipComponent = NULL;
				}
			}

			if (!PilotTargetShipComponent)
			{
				PilotTargetShipComponent = PilotHelper::GetBestTargetComponent(PilotTarget.SpacecraftTarget);
				TimeUntilNextComponentSwitch = 10;
				//FLOGV("%s Select new target component %s ", *Turret->GetReadableName(), *PilotTargetShipComponent->GetReadableName());
			}

			if (!PilotTargetShipComponent)
			{
				PilotTarget.Clear();
				return;
			}
		}
		else
		{
			PilotTargetShipComponent = NULL;
		}
		
		float PredictionDelay = 0;
		float AmmoVelocity = Turret->GetAmmoVelocity() * 100;

		FVector TurretVelocity = 100 * Turret->GetSpacecraft()->GetLinearVelocity();
		FVector AmmoIntersectionPredictedLocation;
		FVector TurretLocation = Turret->GetTurretBaseLocation();

		FVector PilotTargetLocation = (PilotTargetShipComponent ? PilotTargetShipComponent->GetComponentLocation() : PilotTarget.GetActorLocation());

		float AmmoIntersectionPredictedTime = SpacecraftHelper::GetIntersectionPosition(PilotTargetLocation, PilotTarget.GetLinearVelocity(), TurretLocation, TurretVelocity, AmmoVelocity, PredictionDelay, &AmmoIntersectionPredictedLocation);
		FVector PredictedFireTargetLocation;
		if (AmmoIntersectionPredictedTime > 0)
		{
			PredictedFireTargetLocation = AmmoIntersectionPredictedLocation;
		}
		else
		{
			PredictedFireTargetLocation = PilotTargetLocation;
		}
		
		AimAxis = (PredictedFireTargetLocation - TurretLocation).GetUnsafeNormal();
		/*FLOGV("%s Have target AimAxis=%s",*Turret->GetReadableName(),  * AimAxis.ToString());
		FLOGV("%s AmmoIntersectionPredictedTime=%f",*Turret->GetReadableName(),  AmmoIntersectionPredictedTime);
		FLOGV("%s AmmoVelocity=%f",*Turret->GetReadableName(),  AmmoVelocity);*/

		float TargetSize = PilotTarget.GetMeshScale() / 100.f + Turret->GetAimRadius() * 2; // Radius in meters
		FVector DeltaLocation = (PilotTargetLocation-TurretLocation) / 100.f;
		float Distance = DeltaLocation.Size(); // Distance in meters

		//FLOGV("%s Distance=%f",*Turret->GetReadableName(),  Distance);

		// If at range and aligned fire on the target
		// TODO increase tolerance if target is near


		if (AmmoIntersectionPredictedTime > 0 && AmmoIntersectionPredictedTime < 10.f && TimeUntilFireReaction < 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_FlareTurretPilot_Intersect);

			TimeUntilFireReaction = FireReactionTime;
			WantFire = false;

			//FLOG("Near enough");
			FVector FireAxis = Turret->GetFireAxis();
			
			for (int GunIndex = 0; GunIndex < Turret->GetGunCount(); GunIndex++)
			{
				SCOPE_CYCLE_COUNTER(STAT_FlareTurretPilot_Intersect_Gun);

				FVector MuzzleLocation = Turret->GetMuzzleLocation(GunIndex);

				// Compute target Axis for each gun
				FVector AmmoIntersectionLocation;
				float AmmoIntersectionTime = SpacecraftHelper::GetIntersectionPosition(PilotTargetLocation, PilotTarget.GetLinearVelocity(), MuzzleLocation, TurretVelocity , AmmoVelocity, 0, &AmmoIntersectionLocation);
				if (AmmoIntersectionTime < 0)
				{
					// No ammo intersection, don't fire
					continue;
				}
				FVector FireTargetAxis = (AmmoIntersectionLocation - MuzzleLocation).GetUnsafeNormal();
				/*FLOGV("Gun %d FireAxis=%s", GunIndex, *FireAxis.ToString());
				FLOGV("Gun %d FireTargetAxis=%s", GunIndex, *FireTargetAxis.ToString());*/

				float AngularPrecisionDot = FVector::DotProduct(FireTargetAxis, FireAxis);
				float AngularPrecision = FMath::Acos(AngularPrecisionDot);
				float AngularSize = FMath::Atan(TargetSize / Distance);

				/*FLOGV("Gun %d Distance=%f", GunIndex, Distance);
				FLOGV("Gun %d TargetSize=%f", GunIndex, TargetSize);
				FLOGV("Gun %d AngularSize=%f", GunIndex, AngularSize);
				FLOGV("Gun %d AngularPrecision=%f", GunIndex, AngularPrecision);*/
				bool DangerousTarget = IsTargetDangerous(PilotTarget);
				if (AngularPrecision < (DangerousTarget ? AngularSize * 0.25 : AngularSize * 0.2))
				{
					if (!PilotHelper::CheckFriendlyFire(Turret->GetSpacecraft()->GetGame()->GetActiveSector(), PlayerCompany, MuzzleLocation, TurretVelocity, AmmoVelocity, FireTargetAxis, AmmoIntersectionTime, Turret->GetAimRadius()))
					{
						FVector Location = PilotTarget.GetActorLocation();
						FVector Velocity = PilotTarget.GetLinearVelocity() / 100;
						Turret->SetTarget(Location, Velocity);
						WantFire = true;
						break;
					}
					else
					{
						//FLOG("Friendly fire avoidance");
					}
				}
			}
		}
	}
	else
	{
		WantFire = false;
	}
}


/*----------------------------------------------------
	Internal
----------------------------------------------------*/

void UFlareTurretPilot::ProcessTurretTargetSelection()
{

	if (TimeUntilNextTargetSelectionReaction > 0)
	{
		if(PilotTarget.SpacecraftTarget && !PilotTarget.SpacecraftTarget->GetParent()->GetDamageSystem()->IsAlive())
		{
			PilotTarget.Clear();
		}

		return;
	}
	else
	{
		TimeUntilNextTargetSelectionReaction = TargetSelectionReactionTime;
	}


	PilotHelper::PilotTarget OldPilotTargetShip = PilotTarget;

	EFlareCombatTactic::Type Tactic = Turret->GetSpacecraft()->GetParent()->GetCompany()->GetTacticManager()->GetCurrentTacticForShipGroup(EFlareCombatGroup::Capitals);

	PilotTarget = GetNearestHostileTarget(true, Tactic);

	if (Turret->GetWeaponGroup()->Target)
	{
		SCOPE_CYCLE_COUNTER(STAT_FlareTurretPilot_Target);

		AFlareSpacecraft* TargetCandidate = Turret->GetWeaponGroup()->Target;

		if(!TargetCandidate->GetParent()->GetDamageSystem()->IsAlive())
		{
			Turret->GetWeaponGroup()->Target = NULL;
		}
		else
		{
			FVector TargetAxis = (TargetCandidate->GetActorLocation()- Turret->GetTurretBaseLocation()).GetUnsafeNormal();
			if(Turret->IsReacheableAxis(TargetAxis))
			{
				PilotTarget = TargetCandidate;
			}
		}
	}

	if (PilotTarget.IsEmpty())
	{
		PilotTarget = GetNearestHostileTarget(false, Tactic);
	}
}

PilotHelper::PilotTarget UFlareTurretPilot::GetNearestHostileTarget(bool ReachableOnly, EFlareCombatTactic::Type Tactic) const
{
	SCOPE_CYCLE_COUNTER(STAT_FlareTurretPilot_GetNearestHostileShip);

	// For now an host ship is a the nearest host ship with the following critera:
	// - Alive
	// - Is dangerous if needed
	// - From another company
	// - Is the nearest

	float SecurityRadius = 0;

	if (Turret->GetDescription()->WeaponCharacteristics.FuzeType == EFlareShellFuzeType::Proximity)
	{
		 SecurityRadius = Turret->GetDescription()->WeaponCharacteristics.AmmoExplosionRadius + Turret->GetSpacecraft()->GetMeshScale() / 100;
	}

	FVector PilotLocation = Turret->GetTurretBaseLocation();
	float MaxDot = 0;
	PilotHelper::PilotTarget NearestHostileTarget;
	FVector FireAxis = Turret->GetFireAxis();



	struct PilotHelper::TargetPreferences TargetPreferences;
	TargetPreferences.IsLarge = 1;
	TargetPreferences.IsSmall = 1;
	TargetPreferences.IsStation = 0;
	TargetPreferences.IsNotStation = 1;
	TargetPreferences.IsMilitary = 1;
	TargetPreferences.IsNotMilitary = 0.1;
	TargetPreferences.IsDangerous = 1;
	TargetPreferences.IsNotDangerous = 0.01;
	TargetPreferences.IsStranded = 1;
	TargetPreferences.IsNotStranded = 0.5;
	TargetPreferences.IsUncontrollableCivil = 0.0;
	TargetPreferences.IsUncontrollableSmallMilitary = 0.0;
	TargetPreferences.IsUncontrollableLargeMilitary = 0.0;
	TargetPreferences.IsNotUncontrollable = 1;
	TargetPreferences.TargetStateWeight = 1;
	TargetPreferences.MaxDistance = 5000000;
	TargetPreferences.DistanceWeight = 0.1;
	TargetPreferences.AttackTarget = NULL;
	TargetPreferences.AttackTargetWeight = 15;
	TargetPreferences.AttackMeWeight = 10;
	TargetPreferences.LastTarget = PilotTarget;
	TargetPreferences.LastTargetWeight = 5.;
	TargetPreferences.IsBomb = 5.f;
	TargetPreferences.MaxBombDistance = 400000;
	TargetPreferences.IsMeteorite = 0.0001f;

	TargetPreferences.PreferredDirection = FireAxis;
	TargetPreferences.MinAlignement = -1;
	TargetPreferences.AlignementWeight = 1.0;
	TargetPreferences.BaseLocation = PilotLocation;

	TargetPreferences.IsLarge = Turret->GetDescription()->WeaponCharacteristics.AntiLargeShipValue;
	TargetPreferences.IsSmall = Turret->GetDescription()->WeaponCharacteristics.AntiSmallShipValue;
	TargetPreferences.IsStation = Turret->GetDescription()->WeaponCharacteristics.AntiStationValue;



	float AmmoRatio = float(Turret->GetCurrentAmmo()) / Turret->GetMaxAmmo();

	if(AmmoRatio <0.9)
	{
		TargetPreferences.IsUncontrollableSmallMilitary = 0.0;
	}

	if(AmmoRatio < 0.5)
	{
		TargetPreferences.IsNotMilitary = 0.0;
	}


	if (Tactic == EFlareCombatTactic::AttackStations)
	{
		TargetPreferences.IsStation *= 10;
	}
	else if (Tactic == EFlareCombatTactic::AttackMilitary)
	{
		TargetPreferences.IsStation = 0;
	}
	else if (Tactic == EFlareCombatTactic::AttackCivilians)
	{
		TargetPreferences.IsStation = 0;
		TargetPreferences.IsMilitary = 0.1;
		TargetPreferences.IsNotMilitary = 1.0;
		TargetPreferences.IsNotDangerous = 1.0;
	}
	else if (Tactic == EFlareCombatTactic::ProtectMe)
	{
		TargetPreferences.IsStation = 0;
		// Protect me is only available for player ship
		if (Turret->GetSpacecraft()->GetParent()->GetCompany() == Turret->GetSpacecraft()->GetGame()->GetPC()->GetCompany())
		{
			TargetPreferences.AttackTarget = Turret->GetSpacecraft()->GetGame()->GetPC()->GetShipPawn();
			TargetPreferences.AttackTargetWeight = 1.0;
		}
	}


	while (NearestHostileTarget.IsEmpty())
	{
		NearestHostileTarget = PilotHelper::GetBestTarget(Turret->GetSpacecraft(), TargetPreferences);

		if(NearestHostileTarget.IsEmpty())
		{
			// No target
			return PilotHelper::PilotTarget();
		}


		float Distance = (PilotLocation - NearestHostileTarget.GetActorLocation()).Size();
		if (Distance < SecurityRadius * 100)
		{
			TargetPreferences.IgnoreList.Add(NearestHostileTarget);
			NearestHostileTarget.Clear();
			continue;
		}

		FVector TargetAxis = (NearestHostileTarget.GetActorLocation()- PilotLocation).GetUnsafeNormal();

		if (ReachableOnly && !Turret->IsReacheableAxis(TargetAxis))
		{
			TargetPreferences.IgnoreList.Add(NearestHostileTarget);
			NearestHostileTarget.Clear();
			continue;
		}
	}
	return NearestHostileTarget;
}

bool UFlareTurretPilot::IsTargetDangerous(PilotHelper::PilotTarget const& Target) const
{
	if(Target.SpacecraftTarget)
	{
		return Target.SpacecraftTarget->GetParent()->IsMilitary() && !Target.SpacecraftTarget->GetParent()->GetDamageSystem()->IsDisarmed();
	}
	else if(Target.BombTarget)
	{
		return true;
	}

	return false;
}


/*----------------------------------------------------
	Pilot Output
----------------------------------------------------*/

FVector UFlareTurretPilot::GetTargetAimAxis() const
{
	return AimAxis;
}

bool UFlareTurretPilot::IsWantFire() const
{
	return WantFire && !Turret->GetSpacecraft()->GetParent()->GetDamageSystem()->IsDisarmed();
}
