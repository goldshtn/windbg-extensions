#if _WIN64
#define KDEXT_64BIT
#else
#define KDEXT_32BIT
#endif

#ifdef __cplusplus
#define BRIDGE_LINKAGE extern "C"
#else
#define BRIDGE_LINKAGE
#endif

#include <Windows.h>
#include <WDBGEXTS.H>

typedef enum _EVENT_OPERATION_TYPE
{
	EventOperationNone,
	EventOperationOpen,
	EventOperationClose,
	EventOperationCustom
} EVENT_OPERATION_TYPE;

#ifdef _WIN64
#define STACKFRAME EXTSTACKTRACE64
#else
#define STACKFRAME EXTSTACKTRACE32
#endif

BRIDGE_LINKAGE void DisplayStacksForAllObjects(BOOL outstanding);
BRIDGE_LINKAGE void DisplayStacksForObject(ULONG64 object, BOOL outstanding);
BRIDGE_LINKAGE void DisplayAggregateStatistics();
BRIDGE_LINKAGE LONG64 RecordOperation(ULONG64 object, EVENT_OPERATION_TYPE eventOperation, LPCSTR customOperationName, ULONG_PTR weight);
BRIDGE_LINKAGE void DeleteStacksForObject(ULONG64 object);
BRIDGE_LINKAGE void Clear();