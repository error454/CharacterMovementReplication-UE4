// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SideScroller.h"
#include "SideScrollerCharacter.h"

ASideScrollerCharacter::ASideScrollerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName)) 
	/* Be sure to change the default character movement component above! */
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->AttachTo(RootComponent);
	CameraBoom->bAbsoluteRotation = true; // Rotation of the character should not affect rotation of boom
	CameraBoom->TargetArmLength = 500.f;
	CameraBoom->SocketOffset = FVector(0.f,0.f,75.f);
	CameraBoom->RelativeRotation = FRotator(0.f,180.f,0.f);

	// Create a camera and attach to boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->AttachTo(CameraBoom, USpringArmComponent::SocketName);
	SideViewCameraComponent->bUsePawnControlRotation = false; // We don't want the controller rotating the camera

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Face in the direction we are moving..
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->GravityScale = 2.f;
	GetCharacterMovement()->AirControl = 0.80f;
	GetCharacterMovement()->JumpZVelocity = 1000.f;
	GetCharacterMovement()->GroundFriction = 3.f;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->MaxFlySpeed = 600.f;

	CharMovement = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void ASideScrollerCharacter::CheckJumpInput(float DeltaTime)
{
	if (CharMovement)
	{
		CharMovement->PerformAbilities(DeltaTime);
	}

	Super::CheckJumpInput(DeltaTime);
}

void ASideScrollerCharacter::StartSprint()
{
	if (CharMovement)
	{
		CharMovement->bPressedSprint = true;
	}
}

void ASideScrollerCharacter::StopSprint()
{
	if (CharMovement)
	{
		CharMovement->bPressedSprint = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ASideScrollerCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	InputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	InputComponent->BindAction("Sprint", IE_Pressed, this, &ASideScrollerCharacter::StartSprint);
	InputComponent->BindAction("Sprint", IE_Released, this, &ASideScrollerCharacter::StopSprint);
	InputComponent->BindAxis("MoveRight", this, &ASideScrollerCharacter::MoveRight);

	InputComponent->BindTouch(IE_Pressed, this, &ASideScrollerCharacter::TouchStarted);
	InputComponent->BindTouch(IE_Released, this, &ASideScrollerCharacter::TouchStopped);
}

void ASideScrollerCharacter::MoveRight(float Value)
{
	// add movement in that direction
	AddMovementInput(FVector(0.f,-1.f,0.f), Value);
}

void ASideScrollerCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// jump on any touch
	Jump();
}

void ASideScrollerCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	StopJumping();
}

