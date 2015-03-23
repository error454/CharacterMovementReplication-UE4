
#include "SideScroller.h"
#include "MyCharacterMovementComponent.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/* Initialize sprinting flags */
	bPressedSprint = false;
	bIsSprinting = false;
	bWasSprinting = false;
	SprintEndTime = 0.f;
	SprintRechargeTime = 0.f;

	SprintAccel = 5000.f;
	SprintSpeed = 2000.f;
}

bool UMyCharacterMovementComponent::CanSprint() const
{
	if (CharacterOwner && IsMovingOnGround() && !IsCrouching() && (CharacterOwner->GetWorld()->GetTimeSeconds() > SprintRechargeTime))
	{
		return true;
	}
	return false;
}

float UMyCharacterMovementComponent::GetMaxSpeed() const
{
	return bIsSprinting ? SprintSpeed : Super::GetMaxSpeed();
}

float UMyCharacterMovementComponent::GetMaxAcceleration() const
{
	return (bIsSprinting && Velocity.SizeSquared() > FMath::Square<float>(MaxWalkSpeed)) ? SprintAccel : Super::GetMaxAcceleration();
}

void UMyCharacterMovementComponent::PerformAbilities(float DeltaTime)
{
	if (CharacterOwner)
	{
		float WorldTime = CharacterOwner->GetWorld()->GetTimeSeconds();

		/* Keep in mind that this is called both by the server and by the client.
		 * In both cases, we're checking the state of the bPressedSprintflag , the only
		 * difference is that in the server case, bPressedSprint was set via compressed flags.
		 */
		if (bPressedSprint && !bIsSprinting && CanSprint())
		{
			bIsSprinting = true;
		}
		else if (bIsSprinting && WorldTime > SprintEndTime)
		{
			bIsSprinting = false;
		}

		/* Only calculate new timers if the client isn't replaying moves. In the case of the
		 * client replaying moves, the timers below are set via the stored values inside
		 * PrepMoveFor(). Try setting a breakpoint inside PrepMoveFor and set your simulation
		 * latency and packet loss high to see this in action:
		 * Net PktLag=500
		 * Net PktLoss=15
		 */ 
		if (!CharacterOwner->bClientUpdating)
		{
			if (!bWasSprinting && bIsSprinting)
			{
				SprintEndTime = WorldTime + SprintDurationSeconds;
			}
			else if (bWasSprinting && !bIsSprinting)
			{
				SprintRechargeTime = WorldTime + SprintRechargeSeconds;
			}
		}
	}
}

void UMyCharacterMovementComponent::PerformMovement(float DeltaSeconds)
{
	bWasSprinting = bIsSprinting;
	Super::PerformMovement(DeltaSeconds);
}

void UMyCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bPressedSprint = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);

	/* This is a perfectly good place to trigger events based on flags. However you'll notice that I don't.
	 * The MoveAutonomous() function (which only runs on the server) calls:
	 *   UpdateFromCompressedFlags(CompressedFlags);
	 *   CharacterOwner->CheckJumpInput(DeltaTime);
	 *   
	 * You'll notice that in SideScrollerCharacter, CheckJumpInput() directly calls PerformAbilities() to 
	 * trigger the abilities based on this flag that was just set. The reason it is done this way is because
	 * clients take a different route. In their TickComponent() function, they call:
	 *   CharacterOwner->CheckJumpInput(DeltaTime);
	 * 
	 * So CheckJumpInput() ends up being a nice piece of common ground used to trigger abilities.
	 */
}

class FNetworkPredictionData_Client* UMyCharacterMovementComponent::GetPredictionData_Client() const
{
	/* This is straight-up from the UT code. The default values for update distance seem to work fine.
	 * Some comments from me added with ZB prefix.*/
	
	// Should only be called on client in network games
	check(PawnOwner != NULL);
	check(PawnOwner->Role < ROLE_Authority);

	/* ZB: I figured I'd leave this original comment in. I haven't searched for NM_Client and don't know what it is. */

	// once the NM_Client bug is fixed during map transition, should re-enable this
	//check(GetNetMode() == NM_Client);

	if (!ClientPredictionData)
	{
		UMyCharacterMovementComponent* MutableThis = const_cast<UMyCharacterMovementComponent*>(this);

		/* ZB: This is the critical line here. Without these we won't be using our custom network prediction data
		 * client and as a result we won't be using our custom FSavedMove */

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Custom();
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f; // 2X character capsule radius
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

void FSavedMove_Custom::Clear()
{
	Super::Clear();
	bPressedSprint = false;
	SavedSprintEndTime = 0.f;
	SavedSprintRechargeTime = 0.f;
}

uint8 FSavedMove_Custom::GetCompressedFlags() const
{
	/*
	 * Jumping and crouching are originally from the CharacterMovementComponent implementation.
	 * We are preserving them here. Notice that we're OR'ing together all of the flags.
	 */ 
	uint8 Result = 0;

	if (bPressedJump)
	{
		Result |= FLAG_JumpPressed;
	}

	if (bWantsToCrouch)
	{
		Result |= FLAG_WantsToCrouch;
	}

	/*
	 * These are new abilities that we're stuffing into Result. There are 4 pre-defined custom flags:
	 * FLAG_JumpPressed = 0x01,	  Jump pressed
	 * FLAG_WantsToCrouch = 0x02, Wants to crouch
	 * FLAG_Reserved_1 = 0x04,	  Reserved for future use
	 * FLAG_Reserved_2 = 0x08,	  Reserved for future use
	 * FLAG_Custom_0 = 0x10,
	 * FLAG_Custom_1 = 0x20,
	 * FLAG_Custom_2 = 0x40,
	 * FLAG_Custom_3 = 0x80,
	 * 
	 * You might notice that this is a single byte (0x00 - 0x80). It might be easier to see this as the
	 * 8 bits that this byte represents.
	 *  _ _ _ _  _ _ _ _
	 *  7 6 5 4  3 2 1 0
	 *  | | | |  | | | |- FLAG_JumpPressed
	 *  | | | |  | | |--- FLAG_WantsToCrouch
	 *  | | | |  | |----- FLAG_Reserved_1
	 *  | | | |  |------- FLAG_Reserved_2
	 *  | | | |---------- FLAG_Custom_0
	 *  | | |------------ FLAG_Custom_1
	 *  | |-------------- FLAG_Custom_2
	 *  |---------------- FLAG_Custom_3
	 *
	 * In UT, they combine the reserved flags with Custom_0 to get 3 bits of space. Combined that gives them 
	 * 8 unique states they can check (2^3) vs 3. The caveat is a bit of shifting and OR'ing on this end and
	 * shifting and AND'ing on the UpdateFromCompressedFlags end to pull out the bits of interest.
	 */

	/* We only need one flag to hold our sprint state */
	if (bPressedSprint)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

bool FSavedMove_Custom::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bPressedSprint != ((FSavedMove_Custom*)&NewMove)->bPressedSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_Custom::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	/* Saved Move <--- Character Movement Data */
	UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharMov)
	{
		bPressedSprint = CharMov->bPressedSprint;
		SavedSprintEndTime = CharMov->SprintEndTime;
		SavedSprintRechargeTime = CharMov->SprintRechargeTime;
	}
}

void FSavedMove_Custom::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	/* Saved Move ---> Character Movement Data */
	UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharMov)
	{
		CharMov->bPressedSprint = bPressedSprint;
		CharMov->SprintEndTime = SavedSprintEndTime;
		CharMov->SprintRechargeTime = SavedSprintRechargeTime;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_Custom::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Custom());
}
