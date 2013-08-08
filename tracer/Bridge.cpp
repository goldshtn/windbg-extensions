
#include "EventStacks.h"
#include <algorithm>
#include <unordered_map>
#include <set>

#define MAX_STACK_FRAMES 40
#define MAX_SYMBOL_NAME 1024

static EventStacks g_events;

BRIDGE_LINKAGE void DisplayStacksForAllObjects(BOOL outstanding)
{
	g_events.ForEachObject([=](ULONG64 object) { DisplayStacksForObject(object, outstanding); });
}

static void PrintStackFrames(UINT count, ULONG64* frames)
{
	for (UINT i = 0; i < count; ++i)
	{
		CHAR symbol[MAX_SYMBOL_NAME];
		ULONG_PTR displacement;
		GetSymbol((ULONG_PTR)frames[i], symbol, &displacement);
		dprintf("	%s+0x%x\n", symbol, displacement);
	}
}

BRIDGE_LINKAGE void DisplayStacksForObject(ULONG64 object, BOOL outstanding)
{
	const auto& events = g_events.Get(object);
	if (outstanding)
	{
		auto count = std::count_if(events.begin(), events.end(), [](const EventStack& event) {
			return event.EventOperation == EventOperationClose;
		});
		if (count > 0)
			return; //This object has been closed already
	}

	dprintf("------ OPERATIONS FOR OBJECT %llX ------\n", object);
	for (const auto& event : events)
	{
		dprintf("%s operation for object %llx [weight = %x]\n", event.OperationName(), object, event.Weight);
		PrintStackFrames(event.FramesCount, event.Frames);
		dprintf("\n");
	}
	dprintf("\n");
}

//For each stack, display the number of open/other/close operations performed
//from that stack trace. Sort the display by oustanding weight. Note that if
//-keepstack was not used with all !traceclose calls, we will likely have a
//close count of 0 for all stacks, because objects that were completely closed
//would be discarded.
BRIDGE_LINKAGE void DisplayAggregateStatistics()
{
	struct ShortStack
	{
		ShortStack() : Count(0), Frames(nullptr) {}
		ShortStack(UINT count, ULONG64* frames) : Count(count), Frames(frames) {}

		UINT Count;
		ULONG64* Frames;
	};
	struct ShortStats
	{
		ShortStats() : Open(0), Close(0), Other(0), Weight(0) {}

		ULONG64 Open;
		ULONG64 Close;
		ULONG64 Other;
		ULONG64 Weight;
	};
	struct ShortStackHasher
	{
		size_t operator()(ShortStack s)
		{
			size_t result = 18871;
			for (UINT i = 0; i < s.Count; ++i)
				result ^= s.Frames[i];
			return result;
		}
	};
	struct ShortStackComparer
	{
		bool operator()(ShortStack a, ShortStack b)
		{
			if (a.Count != b.Count) return false;
			for (UINT i = 0; i < a.Count; ++i)
				if (a.Frames[i] != b.Frames[i]) return false;
			return true;
		}
	};
	std::unordered_map<ShortStack, ShortStats, ShortStackHasher, ShortStackComparer> statistics;

	ULONG64 totalObjects = 0;
	ULONG64 totalEvents = 0;
	g_events.ForEachObject([&](ULONG64 object)
	{
		totalObjects++;
		for (const EventStack& event : g_events.Get(object))
		{
			++totalEvents;
			ShortStack stack(event.FramesCount, event.Frames);
			ShortStats stats;
			auto iter = statistics.find(stack);
			if (iter != std::end(statistics))
			{
				stats = iter->second;
			}
			switch (event.EventOperation)
			{
			case EventOperationOpen: stats.Open++; stats.Weight += event.Weight; break;
			case EventOperationClose: stats.Close++; break;
			case EventOperationCustom: stats.Other++; break;
			default: break;
			}
			statistics[stack] = stats;
		}
	});

	dprintf("Total objects with events		: 0n%lld\n", totalObjects);
	dprintf("Total events with stack traces	: 0n%lld\n", totalEvents);

	struct WeightComparer
	{
		bool operator()(std::pair<ShortStack, ShortStats> a, std::pair<ShortStack, ShortStats> b)
		{
			return a.second.Weight > b.second.Weight;
		}
	};
	std::multiset<std::pair<ShortStack, ShortStats>, WeightComparer> sortedStatistics;
	for (const auto& stat : statistics)
	{
		sortedStatistics.insert(std::make_pair(stat.first, stat.second));
	}

	ULONG64 count = 0;
	for (const auto& stat : sortedStatistics)
	{
		++count;
		dprintf("----- STACK #%lld OPEN=0n%lld CLOSE=0n%lld OTHER=0n%lld WEIGHT=0n%lld -----\n",
			count, stat.second.Open, stat.second.Close, stat.second.Other, stat.second.Weight);
		PrintStackFrames(stat.first.Count, stat.first.Frames);
		dprintf("\n");
	}
}

BRIDGE_LINKAGE LONG64 RecordOperation(ULONG64 object, EVENT_OPERATION_TYPE eventOperation, LPCSTR customOperationName, ULONG_PTR weight)
{
	STACKFRAME* stackFrames = new STACKFRAME[MAX_STACK_FRAMES];
	StackTrace(0, 0, 0, stackFrames, MAX_STACK_FRAMES);

	ULONG frameCount;
	for (frameCount = 0; frameCount < MAX_STACK_FRAMES; ++frameCount)
	{
		if (stackFrames[frameCount].ReturnAddress == 0)
			break;
	}
	++frameCount;

	LONG64 openCount = g_events.Add(EventStack(object, eventOperation, customOperationName, frameCount, stackFrames, weight));

	delete[] stackFrames;

	return openCount;
}

BRIDGE_LINKAGE void DeleteStacksForObject(ULONG64 object)
{
	g_events.Clear(object);
}

BRIDGE_LINKAGE void Clear()
{
	g_events.ClearAll();
}