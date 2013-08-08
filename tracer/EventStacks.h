#pragma once

#include "Bridge.h"

#include <map>
#include <vector>

struct EventStack
{
	EventStack(ULONG64 objectId, EVENT_OPERATION_TYPE eventOperation, LPCSTR customOperationName, UINT frameCount, STACKFRAME* frames, ULONG_PTR weight)
	{
		ObjectId = objectId;
		EventOperation = eventOperation;

		if (EventOperation == EventOperationCustom)
			CustomOperationName = _strdup(customOperationName);
		else
			CustomOperationName = nullptr;
		
		FramesCount = frameCount;
		Frames = new ULONG64[FramesCount];
		for (UINT i = 0; i < FramesCount; ++i)
		{
			Frames[i] = frames[i].ProgramCounter;
		}

		Weight = weight;
	}

	EventStack() : ObjectId(0), EventOperation(EventOperationNone), CustomOperationName(nullptr),
				   FramesCount(0), Frames(nullptr), Weight(0)
	{
	}

	EventStack(EventStack&& other)
	{
		ObjectId = other.ObjectId;
		EventOperation = other.EventOperation;
		CustomOperationName = other.CustomOperationName;
		FramesCount = other.FramesCount;
		Frames = other.Frames;
		Weight = other.Weight;

		other.CustomOperationName = nullptr;
		other.Frames = nullptr;
	}

	~EventStack()
	{
		if (CustomOperationName != nullptr)
			free((void*)CustomOperationName);
		if (Frames != nullptr)
			delete[] Frames;
	}

	LPCSTR OperationName() const
	{
		switch (EventOperation)
		{
		case EventOperationOpen: return "Open";
		case EventOperationClose: return "Close";
		case EventOperationCustom: return CustomOperationName;
		default: return "None";
		}
	}

	ULONG64 ObjectId; //Handle, memory address, any identifier...
	EVENT_OPERATION_TYPE EventOperation;
	LPCSTR CustomOperationName;
	UINT FramesCount;
	ULONG64* Frames; //Return addresses
	ULONG_PTR Weight; //User-defined, e.g. memory allocation size

private:
	EventStack(const EventStack&);
};

typedef std::vector<EventStack> EventVector;
typedef std::map<ULONG64, EventVector> EventMap;

class EventStacks
{
private:
	std::map<ULONG64, std::vector<EventStack>> events;
	std::map<ULONG64, LONG64> openCounts;

public:
	LONG64 Add(EventStack&& eventStack);
	const EventVector& Get(ULONG64 objectId);
	void Clear(ULONG64 objectId);
	void ClearAll();

	template <typename Fn>
	void ForEachObject(Fn fn)
	{
		for (const auto& key : events)
			fn(key.first);
	}
};

