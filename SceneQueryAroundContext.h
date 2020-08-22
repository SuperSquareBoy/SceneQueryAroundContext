// Created by Daniel Collinson (@SuperSquareBoy) - 2020

#pragma once

#include "CoreMinimal.h"
#include "CoreUObject.h"

#include "SceneQueryAroundContext.generated.h"

class AActor;

struct FSceneQueryNode
{
	FSceneQueryNode();
	FSceneQueryNode(const FVector& location, int16 id);

	FVector WorldLocation = FVector::ZeroVector;
	int16 Id = -1;

	uint32 bCanSeeContext : 1;
	uint32 bOccupied : 1;
};
typedef TArray<FSceneQueryNode> LocationNodeList;
typedef TArray<FSceneQueryNode, TInlineAllocator<64>> InlineLocationNodeList;

USTRUCT(BlueprintType)
struct FSceneQueryAroundContext
{
	GENERATED_BODY()

public:

	FSceneQueryAroundContext();

	// Call this in order to activate the scene query. Supply a context actor (the actor to query around)
	void SetupSceneQuery(AActor* pContextActor);
	// Call every tick to update the query date
	void UpdateSceneQuery(float deltaTime);

	const LocationNodeList& GetListOfNodes() const;
	LocationNodeList& GetListOfNodes();

	AActor* GetContextActor() const;

	// Set a node as occupied to tell other characters it's in use. Will get cleared when data is regenerated
	void SetNodeOccupied(int8 nodeId, bool bVal);

	// Get the node that is closest to a given location.
	FSceneQueryNode* GetClosestFreeNodeToLocation(const FVector& location);

	// Returns a node for a given id. Check for nullptr, nodes will get deleted if they are invalid.
	FSceneQueryNode* GetNodeData(int8 nodeId);

private:

	void UpdateListOfNodes();
	void BatchProjectNodesToNav();
	void NodeLineOfSightChecks();

#if !UE_BUILD_SHIPPING
	void DebugDrawNodes();
#endif

public:

	UPROPERTY(EditDefaultsOnly, Category = Data)
		uint32 EnableDebugDrawing : 1;

	UPROPERTY(EditDefaultsOnly, Category = Data)
		uint32 EnableProjectToNav : 1;

	UPROPERTY(EditDefaultsOnly, Category = Data)
		uint32 EnableLineOfSightChecks : 1;

	// Number of rings around the context actor from inner and outer radius.
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "1"))
		uint8 NumRings = 3;

	// How many nodes to generate per NumRings. More nodes affects performance NumRings*NodesPerRing
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "1"))
		uint8 NodesPerRing = 16;

	// Closest distance to the context actor. centimeters
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "20.0"))
		float InnerRadius = 800.0f;

	// The maximum distance from the context actor. centimeters
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "40.0"))
		float OuterRadius = 1200.0f;

	// How often the data is generated in seconds. performance setting
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "0.0"))
		float RegenerateInterval = 0.3f;

	// location of the context is cached and the next frame we check to see if the location has moved. If moved above this tolerance then update our data
	// if we haven't moved much then the old data is still valid. Performance setting
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "0.0"))
		float LastLocationTolerance = 10.0f;

	// offset applied to our line of sight to context actor. 
	UPROPERTY(EditDefaultsOnly, Category = Data)
		float LineOfSightHeightOffset = 100.0f;

	// Sphere traces are used instead of lines. Lines are too precise. Radius of the sphere traces
	UPROPERTY(EditDefaultsOnly, Category = Data, meta = (ClampMin = "1.0"))
		float SphereTraceRadius = 20.0f;

private:

	TWeakObjectPtr<AActor> m_pContextActor = nullptr;
	FVector m_lastContextLocation = FVector::ZeroVector;

	LocationNodeList m_listOfNodes;

	float m_fIntervalTime = 0.0f;
	bool m_bEnabled = false;
};