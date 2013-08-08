
#define _CRT_SECURE_NO_WARNINGS
#include "Bridge.h"

EXT_API_VERSION g_ExtApiVersion = {
         5 ,
         5 ,
         EXT_API_VERSION_NUMBER ,
         0
     } ;
WINDBG_EXTENSION_APIS ExtensionApis = {0};
USHORT SavedMajorVersion = 0xF;
USHORT SavedMinorVersion = 0;

__declspec(dllexport) LPEXT_API_VERSION /*WDBGAPI*/ ExtensionApiVersion(void)
{
    return &g_ExtApiVersion;
}

__declspec(dllexport) VOID /*WDBGAPI*/ WinDbgExtensionDllInit(
	PWINDBG_EXTENSION_APIS lpExtensionApis,
	USHORT usMajorVersion, 
	USHORT usMinorVersion)
{
     ExtensionApis = *lpExtensionApis;
	 SavedMajorVersion = usMajorVersion;
	 SavedMinorVersion = usMinorVersion;
}

__declspec(dllexport) DECLARE_API (tracehelp)
{
    dprintf("\nTracer debugging extension\n");
	dprintf("By Sasha Goldshtein, blog.sashag.net\n\n");
    dprintf("This debugging extension helps with tracing call stacks for various events.\n");
	dprintf("Usage:\n");
	dprintf("	!traceopen <object>	[weight]						Records an open event\n");
	dprintf("	!traceclose [-keepstack] <object>					Records a close event\n");
	dprintf("	!traceoperation <operation> <object>				Records a custom event\n");
	dprintf("	!traceclear											Clears the trace\n");
	dprintf("	!tracedisplay [-stats | -outstanding | <object> ]	Displays the trace\n\n");
	dprintf("Example for memory allocation tracing (can cause significant slowdown):\n");
	dprintf("	bp ntdll!RtlAllocateHeap \"r $t0 = poi(@esp+0n12); gu; !traceopen @eax @$t0; gc\"\n");
	dprintf("	bp ntdll!RtlFreeHeap \"!traceclose poi(@esp+0n12); gc\"\n\n");
}

//Record the open call stack for the object provided as the first argument.
//If there is a second argument, it is interpreted as the weight of the object.
__declspec(dllexport) DECLARE_API (traceopen)
{
	PSTR token;
	PSTR tokens;
	ULONG_PTR object;
	ULONG_PTR weight = 1;
	
	tokens = _strdup(args);
	token = strtok(tokens, " ");
	if (token != NULL)
	{
		object = GetExpression(token);
		token = strtok(NULL, " ");
		if (token != NULL)
		{
			weight = GetExpression(token);
		}
		RecordOperation(object, EventOperationOpen, NULL, weight);
	}
	free(tokens);
}

//Record the close call stack for the object provided as the first argument.
//If the first argument is "-keepstack", the object is the second argument and
//it means that we should not delete the stacks for this object even if it has
//been closed the same number of times as it has been opened.
__declspec(dllexport) DECLARE_API (traceclose)
{
	ULONG_PTR object;
	BOOL keepStack = FALSE;
	if (strstr(args, "-keepstack") == args)
	{
		keepStack = TRUE;
		args += strlen("-keepstack");
	}

	object = GetExpression(args);
	if ((0 == RecordOperation(object, EventOperationClose, NULL, 1)) && (FALSE == keepStack))
	{
		DeleteStacksForObject(object);
	}
}

//Record an arbitrary call stack for the object provided as the second argument.
//The operation name is the first argument.
__declspec(dllexport) DECLARE_API (traceoperation)
{
	PSTR token;
	PSTR tokens;
	PSTR operationName;
	ULONG_PTR object;
	
	tokens = _strdup(args);
	token = strtok(tokens, " ");
	if (token != NULL)
	{
		operationName = _strdup(token);
		token = strtok(NULL, " ");
		object = GetExpression(token);
		RecordOperation(object, EventOperationCustom, operationName, 1);
		free(operationName);
	}
	free(tokens);
}

//Clear the call stack recording information.
__declspec(dllexport) DECLARE_API (traceclear)
{
	Clear();
}

//Display recorded call stack information. If the first argument is "-outstanding",
//only objects that have not been closed yet are shown. If the first argument is
//"-stats", statistics for different call stacks (aggregated) are displayed. Finally,
//if the first argument is anything else, it is interpreted as an object and only
//information for that object is shown.
__declspec(dllexport) DECLARE_API (tracedisplay)
{
	if (strlen(args) == 0)
	{
		DisplayStacksForAllObjects(FALSE);
	}
	else if (strcmp(args, "-stats") == 0)
	{
		DisplayAggregateStatistics();
	}
	else if (strcmp(args, "-outstanding") == 0)
	{
		DisplayStacksForAllObjects(TRUE);
	}
	else
	{
		ULONG64 object = GetExpression(args);
		DisplayStacksForObject(object, FALSE);
	}
}