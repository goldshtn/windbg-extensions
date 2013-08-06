WCT
===

A WinDbg extension that invokes the Wait Chain Traversal API to display wait chain information for a particular thread or process, or all the processes and threads on the system.

Usage:

```
.load wct_x86.dll
!wct_thread 1fa4
```

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

bkb.py
======

A PyKD script that reconstructs broken call stacks (or dies while trying). This initial version attempts to determine which registers are broken -- stack pointer, base pointer, instruction pointer -- and performs reconstruction accordingly. If all the registers are corrupted, the script attempts to inspect the raw stack using StackLimit and StackBase information from the TEB.

Usage:

```
!py bkb
!py bkb -reset
!py bkb -ignoremissingteb
!py bkb -rawbpwalk
```

The ```-reset``` switch will also set the SP/BP/IP values to the reconstructed results, so that you can continue your analysis with other WinDbg commands.

The ```-ignoremissingteb``` switch will let the script run even if it can't obtain the StackLimit and StackBase values from the TEB (e.g., in certain types of minidumps). If this switch is used, the script requires that one of SP/BP is valid, because there is no way to determine where the thread's stack lies otherwise.

The ```-rawbpwalk``` switch instructs the script to rely on manual stack-walking (EBP chain) instead of using the ```kb``` command in the debugger. There have been some situations where manual stack-walking succeeds where ```kb``` fails. This option is incompatible with x64.

Prior to using this script, you must install [PyKD](http://pykd.codeplex.com/).

traverse_map.script
===================

A WinDbg script that traverses std::map objects.

Usage:

```$$>a< traverse_map.script <map variable> [-c "cmd"]```

where ```cmd``` can reference @$t9, e.g. ```"?? @$t9.second"``` (this is the pair held by the map) and can also reference @$t0, which is the actual tree node pointer.

Examples:

```
$$>a< traverse_map.script my_map -c ".block { .echo ----; ?? @$t9.first; ?? @$t9.second; }"
$$>a< traverse_map.script m -c ".block { .if (@@(@$t9.first) == 8) { .echo ----; ?? @$t9.second } }"
$$>a< traverse_map.script my_map
```

traverse_vector.script
======================

A WinDbg script that traverses std::vector objects.

Usage:

```$$>a< traverse_vector.script <vector variable> ["cmd"]```

where ```cmd``` can reference @$t0, which always points to the current vector element.

cmdtree.txt
===========

A command tree for WinDbg with a collection of handy user-mode and kernel-mode commands, including extensions from SOS for .NET applications.

Usage:

```.cmdtree cmdtree.txt```
