


// This is intended as an example on how to use FSceneQueryAroundContext and won't compile as is.
// It's also a real example of how I use it.

// NOTE: This system wasn't designed to be plug and play, some coding knowledge is needed to use this helper.

// So this is based on the environment query system as part of UE. The EQS at its current state is still early beta and isn't on by default,
// you can't turn it on project wide either, just a local thing to try out. I also wanted more control and wanted to be 100% sure that code like this would 
// be shared between all AI characters that need this data. 
// For example Tank AI characters needing to keep safe distance from the player, I don't want each tank generating safe locations for them self.
// I wanted data to be generated from context of the player actor and all tanks share this same data making it efficient as possible.


// This helper uses the "NavigationSystem" which you'll need to add as a dependency to your build.cs if you're not using that already.


// 1)  First thing is find a place for the structure to live. In my project I have an AI manager which looks after all AI characters in the scene. This manager
// also owns the FSceneQueryAroundContext struct passing in the player character as a context.

UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
	FSceneQueryAroundContext SceneQueryAroundPlayer;

// 2) On begin play of your object call SetupSceneQuery to set everything up. Pass in the context actor. This can be any actor, but my use case was the player.
SceneQueryAroundPlayer.SetupSceneQuery(pPlayerActor);


// 3) On the tick of your object make sure to call UpdateSceneQuery. It's not something that ticks on it's own for efficiency.
void AYourObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SceneQueryAroundPlayer.UpdateSceneQuery(DeltaSeconds);
}


// Now everything is setup, the data just needs to be used some how.

// 4) This is where it can vary a lot. In this simple example, here is a function of how you might want to get a location.
// You can cache the node and use later on. You can call this any time a MoveTo command has finished? Maybe when ever the tank goes
// into the aggressive state? Maybe call it every frame it's in the aggressive state. This is something that can be blueprinted and called
// on the behavior tree when certain actions have finished. Maybe you want extra layers in order to randomly bounce between valid locations?
// Really depends on the use case but here is a function on how to get a valid location and mark as occupied.

void ATank::UpdateShootableNode()
{
	const FSceneQueryNode* pClosestNode = /*AYourObject->*/SceneQueryAroundPlayer.GetClosestFreeNodeToLocation(GetActorLocation());

	if (pClosestNode != nullptr)
	{
		// let the SceneQueryAroundPlayer know about the current node we are using, and free up the previous if that changed.
		SceneQueryAroundPlayer.SetNodeOccupied(pClosestNode->m_id, true);
		SceneQueryAroundPlayer.SetNodeOccupied(m_currentMoveToNode.m_id, false);

		// Store the current node we are at on the Tank object
		m_currentMoveToNode = *pCloestNode;

		// you might want to update some value on the blackboard for a move to task
		pBlackboardComp->SetValueAsVector("ShootableLocation", m_currentMoveToNode.m_navLocation);
	}
}


// 5) Here I just constantly use that cache node to update a blackboard location in this example. You could constantly call the function
// above to constantly get a valid location to make it appear as if they tank is moving to or backing up based on players movement.

void ATank::UpdateTankState(float deltaTime)
{
	// While the tank is aggressive update the ShootableLocation based on cached data
	switch (currentState)
	{
		case EState::Aggressive:
		{
			// The node might not be valid anymore, could've gone outside the nav mesh so find by id first.
			if (const FLocationNode* pNode = SceneQueryAroundPlayer.GetNodeData(m_currentMoveToNode.m_id))
			{
				// Update the AIs ideal range location on the blackboard
				pBlackboardComp->SetValueAsVector("ShootableLocation", pNode->m_navLocation);
			}
		}
		// ....
	}
	// ....
}

// 6) If you need to free up the node that you are using, let's say the tank goes from aggressive to suspicious then we just reset the current node data to free it up for another potentially
// aggressive tank

if (m_currentMoveToNode.m_id != -1)
{
	SceneQueryAroundPlayer.SetNodeOccupied(m_currentMoveToNode.m_id, false);
	m_currentMoveToNode = FSceneQueryNode();
}




// This is pretty much it. Very basic example. My use case was tanks shooting at the player but keeping the shootable trajectory range.
// If the player started to walk up to the tank then the tank slowly backs away. The data is all there for valid locations. How you use those locations
// depends on what you want.





