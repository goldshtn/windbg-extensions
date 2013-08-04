WCT
===

A WinDbg extension that invokes the Wait Chain Traversal API to display wait chain information for a particular thread or process, or all the processes and threads on the system.

Usage:

```
.load wct_x86.dll
!wct_thread 1144
```

Note that the extension takes decimal values (10-based) for process and thread IDs. I intend to fix this at some point so that arbitrary expressions can be provided, like with most other WinDbg commands.

heap_stat.py
============

A PyKD script that enumerates the Windows heap and looks for C++ objects based on their vtable pointers. Anything that resembles a C++ object is then displayed. There are some command line options for filtering the output. The script relies on enumerating vtable symbols for all the DLLs loaded into the process, so it might take a while on the first run. You can use the -save and -load switches to save some time for multiple runs on the same process/dump.

Usage:

```
.load pykd.pyd
!py heap_stat.py
.foreach (obj {!py heap_stat.py -short -type myapp!employee}) { dt myapp!employee obj _salary }
!py heap_stat.py -stat
!py heap_stat.py -save dbginfo.sav
!py heap_stat.py -stat -load dbginfo.sav
```

Prior to using this script, you must install [PyKD](http://pykd.codeplex.com/).