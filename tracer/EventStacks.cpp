#include "EventStacks.h"

LONG64 EventStacks::Add(EventStack&& eventStack)
{
	events[eventStack.ObjectId].emplace_back(std::move(eventStack));
	if (eventStack.EventOperation == EventOperationOpen)
	{
		if (openCounts.count(eventStack.ObjectId) == 0)
		{
			openCounts[eventStack.ObjectId] = 1;
		}
		else
		{
			openCounts[eventStack.ObjectId]++;
		}
	}
	else if (eventStack.EventOperation == EventOperationClose)
	{
		if (openCounts.count(eventStack.ObjectId) == 0)
		{
			dprintf("*** ERROR: Closing object %llx that was not opened yet\n", eventStack.ObjectId);
			openCounts[eventStack.ObjectId] = -1;
		}
		else
		{
			openCounts[eventStack.ObjectId]--;
		}
	}
	return openCounts[eventStack.ObjectId];
}

const EventVector& EventStacks::Get(ULONG64 objectId)
{
	return events[objectId];
}

void EventStacks::Clear(ULONG64 objectId)
{
	openCounts.erase(objectId);
	events.erase(objectId);
}

void EventStacks::ClearAll()
{
	events.clear();
}