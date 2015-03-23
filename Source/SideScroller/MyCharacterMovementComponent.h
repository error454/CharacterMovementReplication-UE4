
#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class SIDESCROLLER_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()
public:
	
	/* 
	 * Tunable Sprint Variables 
	 */

	/* The max speed of sprint */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float SprintSpeed;

	/* The acceleration of the sprint */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float SprintAccel;

	/* How long the sprint lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float SprintDurationSeconds;

	/* How long it takes for the sprint ability to recharge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float SprintRechargeSeconds;

	/*
	 * Untunable Sprint Variables
	 */

	/* World time when sprint should end */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sprint")
	float SprintEndTime;

	/* World time when sprint will be recharged */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sprint")
	float SprintRechargeTime;

	/* True if sprint button has been pressed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sprint")
	bool bPressedSprint;

	/* True if sprinting */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sprint")
	bool bIsSprinting;

	/* True if we were sprint in the last frame */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sprint")
	bool bWasSprinting;

	/* Returns true if we can sprint */
	virtual bool CanSprint() const;

	/* Support for sprint speed. */
	virtual float GetMaxSpeed() const override;

	/* Support for sprint acceleration. */
	virtual float GetMaxAcceleration() const override;

	/* An entry point for the character class to call us to update our added abilities */
	virtual void PerformAbilities(float DeltaTime);
	
	/* If you look at the order of events for CharacterMovementComponent, you'll see that this function is the first thing
	 * called for both client and server when performing movement. We use it as a good place to capture the previous
	 * sprinting state. */
	virtual void PerformMovement(float DeltaSeconds) override;

	/* Read the state flags provided by CompressedFlags and trigger the ability on the server */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/* A necessary override to make sure that our custom FNetworkPredictionData defined below is used */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

/*
 * This custom implementation of FSavedMove_Character is used for 2 things:
 * 1. To replicate special ability flags using the compressed flags
 * 2. To shuffle saved move data around
 */ 
class FSavedMove_Custom : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	/** These flags are used to replicate special abilities via the compressed flags. You can only replicate booleans.*/
	bool bPressedSprint;

	/*
	 * These properties aren't replicated, they're used to replay moves. You want to be sure to throw any timers in 
	 * here so that once a move is replayed you'll have the correct end time for it. 
	 */
	float SavedSprintEndTime;
	float SavedSprintRechargeTime;

	/* Sets the default values for the saved move */
	virtual void Clear() override;

	/* Packs state data into a set of compressed flags. This is undone above in UpdateFromCompressedFlags */
	virtual uint8 GetCompressedFlags() const override;

	/* Checks if an old move can be combined with a new move for replication purposes (are they different or the same) */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

	/* Populates the FSavedMove fields from the corresponding character movement controller variables. This is used when 
	 * making a new SavedMove and the data will be used when playing back saved moves in the event that a correction needs to happen.*/
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;

	/* This is used when the server plays back our saved move(s). This basically does the exact opposite of what 
	 * SetMoveFor does. Here we set the character movement controller variables from the data in FSavedMove. */
	virtual void PrepMoveFor(class ACharacter* Character) override;
};

/*
 * This subclass of FNetworkPredictionData_Client_Character is used to create new copies of
 * our custom FSavedMove_Character class defined above.
 */ 
class FNetworkPredictionData_Client_Custom : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	/* Allocates a new copy of our custom saved move */
	virtual FSavedMovePtr AllocateNewMove() override;
};