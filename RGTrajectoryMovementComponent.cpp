/* ROOIBOT CORE FRAMEWORK
 * Copyright 2023, Rooibot Games, LLC. - All rights reserved.
 *
 * The URGTrajectoryMovementComponent and support files have been made
 * available for the use of other licensees of GRIMTEC's Unreal Engine 5
 * plugin "General Movement Component v2". They may be redistributed to 
 * other GMCv2 licensees, provided this notice remains intact.
 *
 * Questions can be addressed to Rachel Blackman at either 
 * rachel.blackman@rooibot.com or as "Packetdancer" on Discord.
 */

#include "RGTrajectoryMovementComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
URGTrajectoryMovementComponent::URGTrajectoryMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void URGTrajectoryMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void URGTrajectoryMovementComponent::BindReplicationData_Implementation()
{
	Super::BindReplicationData_Implementation();

	BI_InputPresent = BindBool(
		bInputPresent,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::PeriodicAndOnChange_Output,
		EGMC_InterpolationFunction::NearestNeighbour
	);

	BI_InputVelocityOffset = BindSinglePrecisionFloat(
		InputVelocityOffset,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::Default,
		EGMC_SimulationMode::PeriodicAndOnChange_Output,
		EGMC_InterpolationFunction::Linear
	);
}

// Called every frame
void URGTrajectoryMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
												   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHadInput && !IsInputPresent())
	{
		InputStoppedAt = GetTime();
	}
	bHadInput = IsInputPresent();
	
	if (bPrecalculateDistanceMatches)
	{
		UpdateStopPrediction();
		UpdatePivotPrediction();
	}

	if (bTrajectoryEnabled)
	{
		UpdateMovementSamples();
		if (bPrecalculateFutureTrajectory)
		{
			UpdateTrajectoryPrediction();
		}
	}

#if ENABLE_DRAW_DEBUG || WITH_EDITORONLY_DATA
	if (IsTrajectoryDebugEnabled() && !IsNetMode(NM_DedicatedServer))
	{
		const FVector ActorLocation = GetActorLocation_GMC();
		if (bTrajectoryIsStopping || (bDebugHadPreviousStop && GetLinearVelocity_GMC().IsZero()))
		{
			if (bTrajectoryIsStopping) DebugPreviousStop = ActorLocation + PredictedStopPoint;
			DrawDebugSphere(GetWorld(), DebugPreviousStop, 24.f, 12, bTrajectoryIsStopping ? FColor::Blue : FColor::Black, false, bTrajectoryIsStopping ? -1 : 1.f, 0, bTrajectoryIsStopping ? 1.f: 2.f);
			bDebugHadPreviousStop = bTrajectoryIsStopping;
		}
		
		if (bTrajectoryIsPivoting || (bDebugHadPreviousPivot && !DoInputAndVelocityDiffer()))
		{
			if (bTrajectoryIsPivoting) DebugPreviousPivot = ActorLocation + PredictedPivotPoint;
			DrawDebugSphere(GetWorld(), DebugPreviousPivot, 24.f, 12, bTrajectoryIsPivoting ? FColor::Yellow : FColor::White, false, bTrajectoryIsPivoting ? -1 : 2.f, 0, bTrajectoryIsPivoting ? 1.f : 2.f);
			bDebugHadPreviousPivot = bTrajectoryIsPivoting;
		}
		
		if (DoInputAndVelocityDiffer())
		{
			const FVector LinearVelocityDirection = GetLinearVelocity_GMC().GetSafeNormal();
			const FVector AccelerationDirection = UKismetMathLibrary::RotateAngleAxis(LinearVelocityDirection, InputVelocityOffsetAngle(), FVector(0.f, 0.f, 1.f));
			DrawDebugLine(GetWorld(), ActorLocation, ActorLocation + (AccelerationDirection * 120.f), FColor::Yellow, false, -1, 0, 2.f);
		}
				
		if (bTrajectoryEnabled)
		{
			PredictedTrajectory.DrawDebug(GetWorld(), GetPawnOwner()->GetActorTransform());
		}
	}
#endif
	
}

void URGTrajectoryMovementComponent::MovementUpdate_Implementation(float DeltaSeconds)
{
	Super::MovementUpdate_Implementation(DeltaSeconds);

	if (!IsSimulatedProxy() || IsNetMode(NM_Standalone))
	{
		bInputPresent = !GetProcessedInputVector().IsZero();
		InputVelocityOffset = GetAngleDifferenceXY(GetProcessedInputVector(), GetLinearVelocity_GMC());
		CalculatedEffectiveAcceleration = GetTransientAcceleration();
	}

	if (IsSimulatedProxy())
	{
		UpdateCalculatedEffectiveAcceleration();
	}
	
}

void URGTrajectoryMovementComponent::GenSimulationTick_Implementation(float DeltaTime)
{
	Super::GenSimulationTick_Implementation(DeltaTime);

	UpdateCalculatedEffectiveAcceleration();
}

float URGTrajectoryMovementComponent::GetAngleDifferenceXY(const FVector& A, const FVector& B)
{
	const FVector XY=FVector(1.f, 1.f, 0.f);
	return GetAngleDifference(A * XY, B * XY);
}

float URGTrajectoryMovementComponent::GetAngleDifferenceZ(const FVector& A, const FVector& B)
{
	const FVector Z=FVector(0.f, 0.f, 1.f);
	return GetAngleDifference(A * Z, B * Z);
}

float URGTrajectoryMovementComponent::GetAngleDifference(const FVector& A, const FVector& B)
{
	const FVector ANorm = A.GetSafeNormal();
	const FVector BNorm = B.GetSafeNormal();

	return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ANorm, BNorm)));
}

void URGTrajectoryMovementComponent::EnableTrajectoryDebug(bool bEnabled)
{
#if ENABLE_DRAW_DEBUG || WITH_EDITORONLY_DATA
	bDrawDebugPredictions = bEnabled;
#endif
}

bool URGTrajectoryMovementComponent::IsTrajectoryDebugEnabled() const
{
#if ENABLE_DRAW_DEBUG || WITH_EDITORONLY_DATA
	return bDrawDebugPredictions;
#else
	return false;
#endif
}

bool URGTrajectoryMovementComponent::IsInputPresent(bool bAllowGrace) const
{
	if (IsSimulatedProxy() && bAllowGrace)
	{
		return bInputPresent || (GetTime() - InputStoppedAt < 0.01);
	}

	return bInputPresent;
}

void URGTrajectoryMovementComponent::UpdateCalculatedEffectiveAcceleration()
{
	if (GetLinearVelocity_GMC().IsZero())
	{
		CalculatedEffectiveAcceleration = FVector::ZeroVector;
	}
	
	const int32 Idx = GetCurrentMoveHistoryNum();
	if (Idx < 1) return;

	const double SyncedTime = GetTime();
	
	const FGMC_Move LastMove = AccessMoveHistory(Idx - 1);
	if (!LastMove.HasValidTimestamp() || LastMove.MetaData.Timestamp == SyncedTime) return;

	const FVector DeltaV = GetLinearVelocity_GMC() - GetLinearVelocityFromState(LastMove.OutputState);
	CalculatedEffectiveAcceleration = -DeltaV / (SyncedTime - LastMove.MetaData.Timestamp);
}

void URGTrajectoryMovementComponent::UpdateStopPrediction()
{
	PredictedStopPoint = PredictGroundedStopLocation(GetLinearVelocity_GMC(), GetBrakingDeceleration(), GroundFriction);
	bTrajectoryIsStopping = !PredictedStopPoint.IsZero() && !IsInputPresent();
}

void URGTrajectoryMovementComponent::UpdatePivotPrediction()
{
	const FRotator Rotation = GetActorRotation_GMC();
	
	PredictedPivotPoint = PredictGroundedPivotLocation(GetCurrentEffectiveAcceleration(), GetLinearVelocity_GMC(), Rotation, GroundFriction);
	bTrajectoryIsPivoting = !PredictedPivotPoint.IsZero() && IsInputPresent() && DoInputAndVelocityDiffer();
}

bool URGTrajectoryMovementComponent::IsStopPredicted(FVector& OutStopPrediction) const
{
	OutStopPrediction = PredictedStopPoint;
	return bTrajectoryIsStopping;
}

bool URGTrajectoryMovementComponent::IsPivotPredicted(FVector& OutPivotPrediction) const
{
	OutPivotPrediction = PredictedPivotPoint;
	return bTrajectoryIsPivoting;
}

FVector URGTrajectoryMovementComponent::PredictGroundedStopLocation(const FVector& CurrentVelocity,
	float BrakingDeceleration, float Friction)
{
	FVector Result = FVector::ZeroVector;
	
	const FVector GroundedVelocity = CurrentVelocity * FVector(1.f, 1.f, 0.f);
	const FVector GroundedDirection = GroundedVelocity.GetSafeNormal();

	if (const float RealBrakingDeceleration = BrakingDeceleration * Friction; RealBrakingDeceleration > 0.f)
	{
		Result = GroundedDirection * (GroundedVelocity.SizeSquared() / (2 * RealBrakingDeceleration));
	}

	return Result;
}

FVector URGTrajectoryMovementComponent::PredictGroundedPivotLocation(const FVector& CurrentAcceleration,
	const FVector& CurrentVelocity, const FRotator& CurrentRotation, float Friction)
{
	FVector Result = FVector::ZeroVector;
	const FVector EffectiveAcceleration = CurrentAcceleration;
	
	const FVector GroundedVelocity = CurrentVelocity * FVector(1.f, 1.f, 0.f);
	const FVector LocalVelocity = UKismetMathLibrary::LessLess_VectorRotator(GroundedVelocity, CurrentRotation);
	const FVector LocalAcceleration = UKismetMathLibrary::LessLess_VectorRotator(EffectiveAcceleration, CurrentRotation);

	const FVector Acceleration2D = EffectiveAcceleration * FVector(1.f, 1.f, 0.f);
	FVector AccelerationDir2D;
	float AccelerationSize2D;
	Acceleration2D.ToDirectionAndLength(AccelerationDir2D, AccelerationSize2D);

	const float VelocityAlongAcceleration = (GroundedVelocity | AccelerationDir2D);
	const float DotProduct = UKismetMathLibrary::Dot_VectorVector(LocalVelocity, LocalAcceleration);

	if (DotProduct < 0.f && VelocityAlongAcceleration < 0.f)
	{
		const float SpeedAlongAcceleration = -VelocityAlongAcceleration;
		const float Divisor = AccelerationSize2D + 2.f * SpeedAlongAcceleration * Friction;
		const float TimeToDirectionChange = SpeedAlongAcceleration / Divisor;

		const FVector AccelerationForce = EffectiveAcceleration -
			(GroundedVelocity - AccelerationDir2D * GroundedVelocity.Size2D()) * Friction;

		Result = GroundedVelocity * TimeToDirectionChange + 0.5f * AccelerationForce *
			TimeToDirectionChange * TimeToDirectionChange;
	}

	return Result;
}

FRGMovementSampleCollection URGTrajectoryMovementComponent::GetMovementHistory(bool bOmitLatest) const
{
	FRGMovementSampleCollection Result;

	const int32 TrajectoryHistoryCount = MovementSamples.Num();
	if (TrajectoryHistoryCount > 0)
	{
		Result.Samples.Reserve(TrajectoryHistoryCount);

		int32 Counter = 0;
		for (auto& Sample : MovementSamples)
		{
			Counter++;
			if (!bOmitLatest || Counter != TrajectoryHistoryCount - 1)
				Result.Samples.Emplace(Sample);
		}
	}

	return Result;
}

FRGMovementSampleCollection URGTrajectoryMovementComponent::PredictMovementFuture(const FTransform& FromOrigin, bool bIncludeHistory) const
{
	const float TimePerSample = 1.f / TrajectorySimSampleRate;
	const int32 TotalSimulatedSamples = FMath::TruncToInt32(TrajectorySimSampleRate * TrajectorySimSeconds);

	const int32 TotalCollectionSize = TotalSimulatedSamples + 1 + bIncludeHistory ? MovementSamples.Num() : 0;
	
	FRGMovementSample SimulatedSample = LastMovementSample;
	FRGMovementSampleCollection Predictions;
	Predictions.Samples.Reserve(TotalCollectionSize);

	if (bIncludeHistory)
	{
		Predictions.Samples.Append(GetMovementHistory(false).Samples);
	}
	Predictions.Samples.Add(GetMovementSampleFromCurrentState());

	FRotator RotationVelocity;
	FVector PredictedAcceleration;
	GetCurrentAccelerationRotationVelocityFromHistory(PredictedAcceleration, RotationVelocity);
	FRotator RotationVelocityPerSample = RotationVelocity * TimePerSample;

	const float BrakingDeceleration = GetBrakingDeceleration();
	const float BrakingFriction = GroundFriction;
	const float MaxSpeed = GetMaxSpeed();

	const bool bZeroFriction = BrakingFriction == 0.f;
	const bool bNoBrakes = BrakingDeceleration == 0.f;
	
	FVector CurrentLocation = FromOrigin.GetLocation();
	FRotator CurrentRotation = FromOrigin.GetRotation().Rotator();
	FVector CurrentVelocity = GetLinearVelocity_GMC();

	for (int32 Idx = 0; Idx < TotalSimulatedSamples; Idx++)
	{
		if (!RotationVelocityPerSample.IsNearlyZero())
		{
			PredictedAcceleration = RotationVelocityPerSample.RotateVector(PredictedAcceleration);
			CurrentRotation += RotationVelocityPerSample;
			RotationVelocityPerSample.Yaw /= 1.1f;
		}

		if (PredictedAcceleration.IsNearlyZero())
		{
			if (!CurrentVelocity.IsNearlyZero())
			{
				const FVector Deceleration = bNoBrakes ? FVector::ZeroVector : -BrakingDeceleration * CurrentVelocity.GetSafeNormal();

				constexpr float MaxPredictedTrajectoryTimeStep = 1.f / 33.f;
				const FVector PreviousVelocity = CurrentVelocity;

				float RemainingTime = TimePerSample;
				while (RemainingTime >= 1e-6f && !CurrentVelocity.IsZero())
				{
					const float dt = RemainingTime > MaxTimeStep && !bZeroFriction ?
						FMath::Min(MaxPredictedTrajectoryTimeStep, RemainingTime * 0.5f) : RemainingTime;
					RemainingTime -= dt;

					CurrentVelocity = CurrentVelocity + (-BrakingFriction * CurrentVelocity + Deceleration) * dt;
					if ((CurrentVelocity | PreviousVelocity) < 0.f)
					{
						CurrentVelocity = FVector::ZeroVector;
						break;
					}
				}

				if (CurrentVelocity.SizeSquared() < KINDA_SMALL_NUMBER)
				{
					CurrentVelocity = FVector::ZeroVector;
				}
			}
			else
			{
				CurrentVelocity = FVector::ZeroVector;
			}
		}
		else
		{
			const FVector AccelerationDirection = PredictedAcceleration.GetSafeNormal();
			const float Speed = CurrentVelocity.Size();

			CurrentVelocity = CurrentVelocity - (CurrentVelocity - AccelerationDirection * Speed) *
				(BrakingFriction * TimePerSample);

			CurrentVelocity += PredictedAcceleration * TimePerSample;
			CurrentVelocity = CurrentVelocity.GetClampedToMaxSize(MaxSpeed);
		}

		CurrentLocation += CurrentVelocity * TimePerSample;
		
		const FTransform NewTransform = FTransform(CurrentRotation.Quaternion(), CurrentLocation);
		const FTransform NewRelativeTransform = NewTransform.GetRelativeTransform(FromOrigin);

		SimulatedSample = FRGMovementSample();
		SimulatedSample.RelativeTransform = NewRelativeTransform;
		SimulatedSample.RelativeLinearVelocity = FromOrigin.InverseTransformVectorNoScale(CurrentVelocity);
		SimulatedSample.WorldTransform = NewTransform;
		SimulatedSample.WorldLinearVelocity = CurrentVelocity;
		SimulatedSample.AccumulatedSeconds = TimePerSample * (Idx + 1);

		Predictions.Samples.Emplace(SimulatedSample);
	}

	return Predictions;
}

void URGTrajectoryMovementComponent::UpdateTrajectoryPrediction()
{
	PredictedTrajectory = PredictMovementFuture(GetPawnOwner()->GetActorTransform(), true);
}

FRGMovementSample URGTrajectoryMovementComponent::GetMovementSampleFromCurrentState() const
{
	FTransform CurrentTransform = GetPawnOwner()->GetActorTransform();
	const FVector CurrentLocation = GetLowerBound();
	CurrentTransform.SetLocation(CurrentLocation);

	FRGMovementSample Result = FRGMovementSample(CurrentTransform, GetLinearVelocity_GMC());
	Result.ActorWorldRotation = GetPawnOwner()->GetActorRotation();
	if (!LastMovementSample.IsZeroSample())
	{
		Result.ActorDeltaRotation = Result.ActorWorldRotation - LastMovementSample.ActorWorldRotation;
	}

	return Result;
}

void URGTrajectoryMovementComponent::AddNewMovementSample(const FRGMovementSample& NewSample)
{
	const float GameSeconds = UKismetSystemLibrary::GetGameTimeInSeconds(GetWorld());
	if (LastTrajectoryGameSeconds != 0.f)
	{
		const float DeltaSeconds = GameSeconds - LastTrajectoryGameSeconds;
		const float DeltaDistance = NewSample.DistanceFrom(LastMovementSample);
		const FTransform DeltaTransform = LastMovementSample.WorldTransform.GetRelativeTransform(NewSample.WorldTransform);

		for (auto& Sample : MovementSamples)
		{
			Sample.PrependRelativeOffset(DeltaTransform, -DeltaSeconds);
		}

		CullMovementSampleHistory(FMath::IsNearlyZero(DeltaDistance), NewSample);
	}

	MovementSamples.Emplace(NewSample);
	LastMovementSample = NewSample;
	LastTrajectoryGameSeconds = GameSeconds;	
}

void URGTrajectoryMovementComponent::CullMovementSampleHistory(bool bIsNearlyZero, const FRGMovementSample& LatestSample)
{
	const FRGMovementSample FirstSample = MovementSamples.First();
	const float FirstSampleTime = FirstSample.AccumulatedSeconds;

	if (bIsNearlyZero && !FirstSample.IsZeroSample())
	{
		// We were moving, and stopped.
		if (EffectiveTrajectoryTimeDomain == 0.f)
		{
			// Initialize a time horizon while we're motionless, to allow uniform decay to continue.
			EffectiveTrajectoryTimeDomain = FirstSampleTime;
		}
	}
	else
	{
		// We're moving again, so we no longer need a horizon.
		EffectiveTrajectoryTimeDomain = 0.f;
	}

	MovementSamples.RemoveAll([&](const FRGMovementSample& TestSample)
	{
		if (TestSample.IsZeroSample() && LatestSample.IsZeroSample())
		{
			// We don't need duplicate zero motion samples, they just clutter the history.
			return true;
		}

		if (TestSample.AccumulatedSeconds < -TrajectoryHistorySeconds)
		{
			// Sample is too old for the buffer.
			return true;
		}

		if (EffectiveTrajectoryTimeDomain != 0.f && (TestSample.AccumulatedSeconds < EffectiveTrajectoryTimeDomain))
		{
			// Time horizon is in effect, prune everything before our zero motion moment.
			return true;
		}

		// Keep the sample
		return false;
	});	
}

void URGTrajectoryMovementComponent::GetCurrentAccelerationRotationVelocityFromHistory(FVector& OutAcceleration,
	FRotator& OutRotationVelocity) const
{
	const auto HistoryArray = MovementSamples;
	if (HistoryArray.IsEmpty())
	{
		OutAcceleration = FVector::ZeroVector;
		OutRotationVelocity = FRotator::ZeroRotator;
		return;
	}
	
	for (int32 Idx = HistoryArray.Num() - 1; Idx >= 0; Idx--)
	{
		const FRGMovementSample Sample = HistoryArray[Idx];

		if (LastMovementSample.AccumulatedSeconds - Sample.AccumulatedSeconds >= 0.01f)
		{
			OutRotationVelocity = LastMovementSample.GetRotationVelocityFrom(Sample);
			OutAcceleration = LastMovementSample.GetAccelerationFrom(Sample);
			return;
		}
	}	
}

void URGTrajectoryMovementComponent::UpdateMovementSamples_Implementation()
{
	const float GameSeconds = UKismetSystemLibrary::GetGameTimeInSeconds(GetWorld());
	if (GameSeconds - LastTrajectoryGameSeconds > SMALL_NUMBER)
	{
		AddNewMovementSample(GetMovementSampleFromCurrentState());
	}	
}


