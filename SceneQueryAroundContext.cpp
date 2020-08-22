// Created by Daniel Collinson (@SuperSquareBoy) - 2020

#include "SceneQueryAroundContext.h"

#include "GameFramework/Actor.h"

#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

DECLARE_STATS_GROUP(TEXT("SceneQueryAroundContext"), STATGROUP_SceneQueryAroundContext, STATCAT_Advanced)

DECLARE_CYCLE_STAT(TEXT("UpdateListOfNodes"), STAT_UpdateListOfNodes, STATGROUP_SceneQueryAroundContext);
DECLARE_CYCLE_STAT(TEXT("BatchNavProjection"), STAT_BatchNavProjection, STATGROUP_SceneQueryAroundContext);
DECLARE_CYCLE_STAT(TEXT("NodeLineOfSightChecks"), STAT_NodeLineOfSightChecks, STATGROUP_SceneQueryAroundContext);

//-------------------------------------------------------------------------------------------------------------------------------

FSceneQueryNode::FSceneQueryNode()
{
	bCanSeeContext = true;
	bOccupied = false;
}

FSceneQueryNode::FSceneQueryNode(const FVector& location, int16 id)
	: WorldLocation(location)
	, Id(id)
{
	bCanSeeContext = true;
	bOccupied = false;
}

//-------------------------------------------------------------------------------------------------------------------------------

FSceneQueryAroundContext::FSceneQueryAroundContext()
{
	EnableDebugDrawing = true;
	EnableProjectToNav = true;
	EnableLineOfSightChecks = true;
}

void FSceneQueryAroundContext::SetupSceneQuery(AActor* pContextActor)
{
	m_pContextActor = pContextActor;
	m_bEnabled = m_pContextActor.IsValid();

	m_listOfNodes.Reserve(NodesPerRing * NumRings);

	ensureAlwaysMsgf(InnerRadius < OuterRadius, TEXT("Inner radius cannot be greater than OuterRadius. Disabling..."));
	
	m_bEnabled &= InnerRadius < OuterRadius;
}

void FSceneQueryAroundContext::UpdateSceneQuery(float deltaTime)
{
	if (m_bEnabled)
	{
#if !UE_BUILD_SHIPPING
		DebugDrawNodes();
#endif

		// Main update
		if (m_pContextActor.IsValid())
		{
			m_fIntervalTime -= deltaTime;

			if (m_fIntervalTime <= 0.0f)
			{
				UpdateListOfNodes();
				m_fIntervalTime = RegenerateInterval;
			}
		}
	}
}

const LocationNodeList& FSceneQueryAroundContext::GetListOfNodes() const
{
	return m_listOfNodes;
}

LocationNodeList& FSceneQueryAroundContext::GetListOfNodes()
{
	return m_listOfNodes;
}

AActor* FSceneQueryAroundContext::GetContextActor() const
{
	return m_pContextActor.Get();
}

void FSceneQueryAroundContext::UpdateListOfNodes()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateListOfNodes);

	const FVector contextLocation = m_pContextActor->GetActorLocation();

	// If we haven't moved for *Tolerance* then no point regenerating
	if (m_lastContextLocation.Equals(contextLocation, LastLocationTolerance))
	{
		return;
	}

	m_listOfNodes.Empty();

	const static float two_pi = 2.0f * PI;

	const float radiusDelta = (OuterRadius - InnerRadius) / (NumRings - 1);
	const float angleDelta = two_pi / NodesPerRing;

	const float angleOffset = angleDelta / NumRings; // used to spread the rings a little

	float currentRadius = InnerRadius;
	for (int32 ringIdx = 0; ringIdx < NumRings; ++ringIdx, currentRadius += radiusDelta)
	{
		float currentAngle = angleOffset * ringIdx;

		for (int32 nodeIdx = 0; nodeIdx < NodesPerRing; ++nodeIdx, currentAngle += angleDelta)
		{
			const FVector location(currentRadius * FMath::Sin(currentAngle), currentRadius * FMath::Cos(currentAngle), 0.0f);

			const int16 id = (int16)(nodeIdx + (NodesPerRing * ringIdx));
			m_listOfNodes.Emplace(contextLocation + location, id);
		}
	}

	if (EnableProjectToNav)
	{
		BatchProjectNodesToNav();
	}
	if (EnableLineOfSightChecks)
	{
		NodeLineOfSightChecks();
	}

	m_lastContextLocation = contextLocation;
}

void FSceneQueryAroundContext::SetNodeOccupied(int8 nodeId, bool bVal)
{
	if (FSceneQueryNode* pNode = GetNodeData(nodeId))
	{
		pNode->bOccupied = bVal;
	}
}
 
FSceneQueryNode* FSceneQueryAroundContext::GetClosestFreeNodeToLocation(const FVector& location)
{
	const int32 size = m_listOfNodes.Num();
	float fClosestNodeDist = FLT_MAX;

	FSceneQueryNode* pFoundNode = nullptr;

	for (int32 i = 0; i < size; ++i)
	{
		if (m_listOfNodes[i].bCanSeeContext && m_listOfNodes[i].bOccupied == false)
		{
			const float fDistSq = FVector::DistSquared(location, m_listOfNodes[i].WorldLocation);
			if (fDistSq < fClosestNodeDist)
			{
				fClosestNodeDist = fDistSq;
				pFoundNode = &m_listOfNodes[i];
			}
		}
	}

	return pFoundNode;
}

FSceneQueryNode* FSceneQueryAroundContext::GetNodeData(int8 nodeId)
{
	const int listSize = m_listOfNodes.Num();

	for (int i = 0; i < listSize; ++i)
	{
		if (m_listOfNodes[i].Id == nodeId)
		{
			return &m_listOfNodes[i];
		}
	}

	return nullptr;
}

void FSceneQueryAroundContext::BatchProjectNodesToNav()
{
	SCOPE_CYCLE_COUNTER(STAT_BatchNavProjection);

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(m_pContextActor->GetWorld());

	if (NavSys == nullptr)
	{
		ensureAlwaysMsgf(false, TEXT("There is no navigation data, do you want EnableProjectToNav to be set to true. If so does the level have a navmesh? List of nodes will be cleared."));
		m_listOfNodes.Empty();

		return;
	}

	if (const INavAgentInterface* pNavAgentInterface = Cast<INavAgentInterface>(m_pContextActor))
	{
		const FNavAgentProperties& rNavAgentProps = pNavAgentInterface->GetNavAgentPropertiesRef();

		if (ANavigationData* pNavData = NavSys->GetNavDataForProps(rNavAgentProps, pNavAgentInterface->GetNavAgentLocation()))
		{
			FSharedConstNavQueryFilter navigationFilter = UNavigationQueryFilter::GetQueryFilter(*pNavData, m_pContextActor.Get(), nullptr);

			TArray<FNavigationProjectionWork> workload;
			workload.Reserve(m_listOfNodes.Num());

			for (const FSceneQueryNode& node : m_listOfNodes)
			{
				workload.Add(FNavigationProjectionWork(node.WorldLocation));
			}

			static const FVector ProjectionExtent(16.0f, 16.0f, 512.0f);
			pNavData->BatchProjectPoints(workload, ProjectionExtent, navigationFilter);

			for (int32 i = workload.Num() - 1; i >= 0; --i)
			{
				if (workload[i].bResult)
				{
					m_listOfNodes[i].WorldLocation = workload[i].OutLocation;
				}
				else
				{
					m_listOfNodes.RemoveAt(i, 1, false);
				}
			}
		}
	}
}

void FSceneQueryAroundContext::NodeLineOfSightChecks()
{
	SCOPE_CYCLE_COUNTER(STAT_NodeLineOfSightChecks);

	FCollisionObjectQueryParams objectQueryParams = FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECollisionChannel::ECC_WorldStatic) |
		ECC_TO_BITFIELD(ECollisionChannel::ECC_Pawn) | ECC_TO_BITFIELD(ECollisionChannel::ECC_Destructible));

	static FName s_LSQTag("FSceneQueryAroundContext::NodeLineOfSightChecks");
	FCollisionQueryParams params(s_LSQTag);

	for (int32 i = 0; i < m_listOfNodes.Num(); ++i)
	{
		FVector startLoc = m_listOfNodes[i].WorldLocation;
		startLoc.Z += LineOfSightHeightOffset;

		FHitResult hitResult;
		m_pContextActor->GetWorld()->SweepSingleByObjectType(hitResult, startLoc, m_pContextActor->GetActorLocation(), FQuat::Identity,
			objectQueryParams, FCollisionShape::MakeSphere(SphereTraceRadius), params);

		const AActor* pHitActor = hitResult.Actor.Get();
		const bool bFoundContextActor = pHitActor ? pHitActor->IsOwnedBy(m_pContextActor.Get()) : false;

		m_listOfNodes[i].bCanSeeContext = bFoundContextActor;
	}
}

#if !UE_BUILD_SHIPPING
void FSceneQueryAroundContext::DebugDrawNodes()
{
	if (EnableDebugDrawing && m_pContextActor.IsValid())
	{
		for (int32 i = 0; i < m_listOfNodes.Num(); ++i)
		{
			DrawDebugSphere(m_pContextActor->GetWorld(), m_listOfNodes[i].WorldLocation, 20.0f, 12, m_listOfNodes[i].bCanSeeContext ? FColor::Green : FColor::Red,
				false, -1.0f, 0, 1.0f);
		}
	}
}
#endif