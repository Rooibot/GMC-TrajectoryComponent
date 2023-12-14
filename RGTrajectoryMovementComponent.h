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
#pragma once

#include "CoreMinimal.h"
#include "GMCOrganicMovementComponent.h"
#include "RGMovementSample.h"
#include "Containers/RingBuffer.h"
#include "RGTrajectoryMovementComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ROOICORE_API URGTrajectoryMovementComponent : public UGMC_OrganicMovementCmp
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URGTrajectoryMovementComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// GMC Overrides
	virtual void BindReplicationData_Implementation() override;
	virtual void MovementUpdate_Implementation(float DeltaSeconds) override;
	virtual void GenSimulationTick_Implementation(float DeltaTime) override;

	// Utilities

	/// Returns the angle, in degrees, by which vector B differs from vector A on the XY plane.
	/// In other words, the amount by which the yaw between them changes.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	static float GetAngleDifferenceXY(const FVector& A, const FVector& B);

	/// Returns the angle, in degrees, by which vector B differs from vector A on the Z plane.
	/// In other words, the ascent or descent.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	static float GetAngleDifferenceZ(const FVector& A, const FVector& B);

	/// Returns the angle, in degrees, by which vector B differs from vector A along a plane
	/// defined by their cross product.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	static float GetAngleDifference(const FVector& A, const FVector& B);
	
	// Debug
	
	/// If called in a context where the DrawDebug calls are disabled, this function will do nothing.
	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	void EnableTrajectoryDebug(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Movement Trajectory")
	bool IsTrajectoryDebugEnabled() const;
	
	// Trajectory state functionality (input presence, acceleration synthesis for simulated proxies, etc.)
#pragma region Trajectory State
public:

	/// If true, this pawn is actively being provided with an input acceleration.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	bool IsInputPresent(bool bAllowGrace = false) const;

	/// If true, velocity differs enough from input acceleration that we're likely to pivot.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	bool DoInputAndVelocityDiffer() const { return FMath::Abs(InputVelocityOffset) > 90.f; }

	/// The angle, in degrees, that velocity differs from provided input (on the XY plane).
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	float InputVelocityOffsetAngle() const { return InputVelocityOffset; }

	/// The current effective acceleration we're moving at. This differs from input acceleration, as it's
	/// calculated from movement history.
	UFUNCTION(BlueprintPure, Category="Movement Trajectory")
	FVector GetCurrentEffectiveAcceleration() const { return CalculatedEffectiveAcceleration; }

private:

	void UpdateCalculatedEffectiveAcceleration();
	
	// Input state, for stop detection
	bool bInputPresent { false };
	int32 BI_InputPresent { -1 };

	// The angle, in degrees, by which our input acceleration and current linear velocity differ on the XY plane.
	float InputVelocityOffset { 0.f };
	int32 BI_InputVelocityOffset { -1 };

	// Current effective acceleration, for pivot calculation
	FVector CalculatedEffectiveAcceleration { 0.f };

#pragma endregion	

	// Stop/pivot point prediction, for distance matching animation.
#pragma region Stop/Pivot Prediction
public:
	/// Calls the stop point prediction logic; the result will be cached in the PredictedStopPoint and
	/// TrajectoryIsStopping properties.
	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	void UpdateStopPrediction();

	/// Calls the pivot point prediction logic; the result will be cached in the PredictedPivotPoint and
	/// TrajectoryIsPivoting properties.
	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	void UpdatePivotPrediction();

	/// Check whether a stop is predicted, and store the prediction in OutStopPrediction. Only valid
	/// if UpdateStopPrediction has been called, or PrecalculateDistanceMatches is true.
	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	bool IsStopPredicted(FVector &OutStopPrediction) const;

	/// Check whether a pivot is predicted, and store the prediction in OutPivotPrediction. Only valid
	/// if UpdatePivotPrediction has been called, or PrecalculateDistanceMatches is true.
	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	bool IsPivotPredicted(FVector &OutPivotPrediction) const;

	/// If true, this component will pre-calculate stop and pivot predictions, so that they can be easily accessed
	/// in a thread-safe manner without needing to manually call the calculations each time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement Trajectory|Precalculations")
	bool bPrecalculateDistanceMatches { true };

	UFUNCTION(BlueprintPure, Category="RooiCore Trajectory Matching", meta=(ToolTip="Returns a predicted point relative to the actor where they'll come to a stop.", BlueprintThreadSafe))
	static FVector PredictGroundedStopLocation(const FVector& CurrentVelocity, float BrakingDeceleration, float Friction);

	UFUNCTION(BlueprintPure, Category="RooiCore Trajectory Matching", meta=(ToolTip="Returns a predicted point relative to the actor where they'll finish a pivot.", NotBlueprintThreadSafe))
	static FVector PredictGroundedPivotLocation(const FVector& CurrentAcceleration, const FVector& CurrentVelocity, const FRotator& CurrentRotation, float Friction);


private:

	/// Used by simulated proxies for a very small 'grace period' on predicting pivots, to prevent false negatives.
	bool bHadInput { false };
	
	/// Used by simulated proxies for a very small 'grace period' on predicting pivots, to prevent false negatives.
	double InputStoppedAt { 0 };
	
	/// Whether or not a stop is imminent. Only valid when UpdateStopPrediction has been called,
	/// or PrecalculateDistanceMatches is true.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement Trajectory", meta=(AllowPrivateAccess=true))
	bool bTrajectoryIsStopping { false };

	/// A position relative to our location where we predict a stop. Only valid when UpdateStopPrediction
	/// has been called, or PrecalculateDistanceMatches is true.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement Trajectory", meta=(AllowPrivateAccess=true))
	FVector PredictedStopPoint { 0.f };

	/// Whether or not a pivot is imminent. Only valid when UpdatePivotPrediction has been called,
	/// or PrecalculateDistanceMatches is true.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement Trajectory", meta=(AllowPrivateAccess=true))
	bool bTrajectoryIsPivoting { false };

	/// A position relative to our location where we predict a pivot. Only valid when UpdatePivotPrediction has
	/// been called, or PrecalculateDistanceMatches is true.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement Trajectory", meta=(AllowPrivateAccess=true))
	FVector PredictedPivotPoint { 0.f };

#if WITH_EDITORONLY_DATA
	/// Should we start with trajectory debug enabled? Only valid in editor.
	UPROPERTY(EditDefaultsOnly, Category="Movement Trajectory", meta=(AllowPrivateAccess=true))
	bool bDrawDebugPredictions { false };
#endif
	
#if ENABLE_DRAW_DEBUG || WITH_EDITORONLY_DATA
	/// For debug rendering
	bool bDebugHadPreviousStop { false };
	FVector DebugPreviousStop { 0.f };
	bool bDebugHadPreviousPivot { false };
	FVector DebugPreviousPivot { 0.f };
#endif
	
#pragma endregion 

	// Trajectory history and prediction
#pragma region Trajectory History
public:

	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	FRGMovementSampleCollection GetMovementHistory(bool bOmitLatest) const;

	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	FRGMovementSampleCollection PredictMovementFuture(const FTransform& FromOrigin, bool bIncludeHistory) const;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement Trajectory")
	bool bTrajectoryEnabled { true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement Trajectory|Precalculations")
	bool bPrecalculateFutureTrajectory { true };
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement Trajectory")
	int32 MaxTrajectorySamples = { 200 };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement Trajectory")
	float TrajectoryHistorySeconds { 2.f };
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement Trajectory")
	int32 TrajectorySimSampleRate = { 30 };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement Trajectory")
	float TrajectorySimSeconds = { 1.f };

	/// The last predicted trajectory. Only valid if PrecalculateFutureTrajectory is true, or
	/// UpdateTrajectoryPrediction has been manually called.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement Trajectory")
	FRGMovementSampleCollection PredictedTrajectory;
	
protected:

	UFUNCTION(BlueprintCallable, Category="Movement Trajectory")
	void UpdateTrajectoryPrediction();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Movement Trajectory")
	FRGMovementSample GetMovementSampleFromCurrentState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Movement Trajectory")
	void AddNewMovementSample(const FRGMovementSample& Sample);

	void CullMovementSampleHistory(bool bIsNearlyZero, const FRGMovementSample& LatestSample);

	UFUNCTION(BlueprintNativeEvent, Category="Movement Trajectory")
	void UpdateMovementSamples();
	
	void GetCurrentAccelerationRotationVelocityFromHistory(FVector& OutAcceleration, FRotator& OutRotationVelocity) const;

	
private:

	TRingBuffer<FRGMovementSample> MovementSamples;
	FRGMovementSample LastMovementSample;
	
	float LastTrajectoryGameSeconds { 0.f };

	float EffectiveTrajectoryTimeDomain { 0.f };	
	
#pragma endregion
	
};
