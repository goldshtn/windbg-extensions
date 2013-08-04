from pykd import *
import re
import sys
import pickle

# TODO list:
#	1) better parameter parsing and validation
#	2) cleaner printing code

stats_only = False
save_info = False
short_output = False

pointer_size = 4
pointer_format = "%08x"
if getProcessorMode() == "X64":
	pointer_size = 8
	pointer_format = "%016x"

if '-stat' in sys.argv:
	stats_only = True

potential_save_file = ""

if '-save' in sys.argv:
	save_info = True
	potential_save_file = sys.argv[sys.argv.index('-save')+1]

if '-load' in sys.argv:
	if save_info:
		dprintln("Error: can't use -load together with -save")
		exit()
	potential_save_file = sys.argv[sys.argv.index('-load')+1]

if '-short' in sys.argv:
	short_output = True

min_count = 0
if '-min' in sys.argv:
	min_count = int(sys.argv[sys.argv.index('-min')+1])

type_filter = ""
if '-type' in sys.argv:
	type_filter = sys.argv[sys.argv.index('-type')+1]

if '-help' in sys.argv:
	dprintln("")
	dprintln("Usage:")
	dprintln("	!py %s [-stat] [-help] [-short] [-type <typename>] [-min <count>] [-<save|load> <cache_filename>]" % sys.argv[0])
	dprintln("")
	dprintln("	-stat	displays statistics only at the end of the run")
	dprintln("	-short 	displays only object addresses, for scripting with .foreach and similar commands")
	dprintln("	-help	displays usage information")
	dprintln("	-type	filters output to the specified type(s) only - accepts a regular expression")
	dprintln("	-min	filters statistics output to types that have at least that many instances")
	dprintln("	-save	at the end of the run, saves type information to a file to make subsequent runs faster")
	dprintln("	-load	read type information from a file to make the run faster")
	dprintln("")
	exit()

vftables_by_address = {}
vftables_by_type = {}
typesize_by_type = {}

if not save_info and len(potential_save_file) > 0:
	if not short_output:
		dprint("Loading type information from save file %s..." % potential_save_file)
	file = open(potential_save_file, 'rb')
	(vftables_by_address, vftables_by_type, typesize_by_type) = pickle.load(file)
	file.close()
	if not short_output:
		dprintln("DONE")
else:
	if not short_output:
		dprint("Running x /2 *!*`vftable' command...")
	vftables = dbgCommand("x /2 *!*`vftable'").split('\n')
	if not short_output:
		dprintln("DONE")
	for vftable in vftables:
		parts = [w.lstrip().rstrip() for w in vftable.replace("::`vftable'", "").split(' ', 1)]
		if len(parts) < 2: continue
		(address, type) = parts
		address = address.replace('`', '')
		address_ptr = long(address,16)
		vftables_by_address[address_ptr] = type
		vftables_by_type[type] = address_ptr

if not short_output:
	dprint("Running !heap -h 0 command...")
heap_output = dbgCommand('!heap -h 0').split('\n')
if not short_output:
	dprintln("DONE")

stats = {}

if not short_output:
	dprintln("Enumerating %d heap blocks" % len(heap_output))
blocks_done = 0
for heap_block in heap_output:
	blocks_done += 1
	if stats_only and blocks_done % 100 == 0 and not short_output:
		dprintln("	Enumerated %d heap blocks" % blocks_done)
	# example block output: 00e3f088: 00080 . 00078 [101] - busy (70)
	match = re.match(r'\s+([0-9a-f]+): [0-9a-f]+ \. [0-9a-f]+ \[[0-9a-f]+\] - busy \(([0-9a-f]+)\)', heap_block)
	if match:
		address = long(match.group(1),16)
		size = long(match.group(2),16)
		ptr = address - pointer_size
		while ptr < address+size:
			ptr += pointer_size
			try:
				vftable_candidate = ptrPtr(ptr)
			except:
				continue
			if vftable_candidate in vftables_by_address:
				type_name = vftables_by_address[vftable_candidate]
				if len(type_filter) > 0 and not re.match(type_filter, type_name):
					continue
				if not stats_only:
					if short_output:
						dprintln(pointer_format % ptr)
					else:
						dprintln((pointer_format + "\t%s") % (ptr, type_name))
				if type_name in stats:
					stats[type_name] += 1
				else:
					stats[type_name] = 1

if not short_output:
	dprintln("")
	dprintln("Statistics:")
	dprintln("%50s\t%10s\t%s" % ("Type name", "Count", "Size"))
	for type in sorted(stats, key=stats.get, reverse=True):
		if stats[type] < min_count or (len(type_filter) > 0 and not re.match(type_filter, type)):
			continue
		if not type in typesize_by_type:
			try:
				type_info = typeInfo(type)
				typesize_by_type[type] = type_info.size()
			except:
				# some types aren't included in public symbols, so we can't get their type
				typesize_by_type[type] = None
		size = "Unknown"
		if typesize_by_type[type] is not None:
			size = stats[type]*typesize_by_type[type]
		dprintln("%50s\t%10d\t%s" % (type, stats[type], size))

if not short_output:
	dprintln("")
if save_info and len(potential_save_file) > 0:
	if not short_output:
		dprint("Saving type information and vtables to file %s..." % potential_save_file)
	file = open(potential_save_file, 'wb')
	pickle.dump((vftables_by_address, vftables_by_type, typesize_by_type), file)
	file.close()
	if not short_output:
		dprintln("DONE")