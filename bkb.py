from pykd import *
import sys

dprintln("")

force = "-force" in sys.argv
resetRegisters = "-reset" in sys.argv
ignoreMissingTeb = "-ignoremissingteb" in sys.argv
rawBpWalk = "-rawbpwalk" in sys.argv

is64Bit = False
SP_REG = "esp"
BP_REG = "ebp"
IP_REG = "eip"
PTR_SIZE = 4
if getProcessorMode() == "X64":
	is64Bit = True
	SP_REG = "rsp"
	BP_REG = "rbp"
	IP_REG = "rip"
	PTR_SIZE = 8

if rawBpWalk and is64Bit:
	dprintln("Raw BP walk is not compatible with x64")
	exit(1)
if rawBpWalk and resetRegisters:
	dprintln("Raw BP walk is not compatible with -reset")
	exit(1)

stackBase = 0
stackLimit = 0
missingTeb = False

teb = dbgCommand("!teb").split('\n')
if len(teb) < 4:
	missingTeb = True
	if not ignoreMissingTeb:
		dprintln("Error reading TEB -- you might be looking at a minidump")
		exit(1)
else:
	stackBase = long(teb[2].split()[1], 16)
	stackLimit = long(teb[3].split()[1], 16)

dprintln("<u>Initial analysis</u>:", True)
dprintln("")
if not missingTeb:
	dprintln("<b>StackBase</b>  = %16x" % stackBase, True)
	dprintln("<b>StackLimit</b> = %16x" % stackLimit, True)
	dprintln("")
else:
	dprintln("<b>TEB is missing</b>", True)

def inStackLimits(p):
	return missingTeb or (p <= stackBase and p > stackLimit)

sp = reg(SP_REG)
bp = reg(BP_REG)
ip = reg(IP_REG)

spOk = isValid(sp) and inStackLimits(sp)
bpOk = is64Bit or (isValid(bp) and inStackLimits(bp))

ipOk = isValid(ip)
if ipOk:
	sym = findSymbol(ip)
	if '!' not in sym and '+' not in sym:
		ipOk = False #IP is not in any known module, this is suspicious but could be JS/.NET/etc.

dprintln("<b>SP</b> = %16x, OK = %d" % (sp, spOk), True)
dprintln("<b>BP</b> = %16x, OK = %d" % (bp, bpOk), True)
dprintln("<b>IP</b> = %16x, OK = %d" % (ip, ipOk), True)
dprintln("")

if spOk and bpOk and ipOk and not force:
	dprintln("Everything looks OK. Use the <i>-force</i> switch to force reconstruction anyway.", True)
	dprintln("If you force reconstruction, we will look for anything within stack limits.")
	exit(2)

if not spOk and not bpOk and missingTeb:
	dprintln("The TEB is missing and both SP and BP are corrupted. Analysis is not possible.")
	exit(3)

# Temporary hack because we need a specific output format
dmlWasOn = "on by default" in dbgCommand(".prefer_dml")
if dmlWasOn:
	dbgCommand(".prefer_dml 0")

def stackFromRawBpWalk(bp, sp, ip):
	result = "ChildEBP RetAddr  Args to Child              \n"
	while isValid(bp):
		sym = findSymbol(ip)
		src = ()
		try:
			src = getSourceLine(ip)
		except:
			srcText = ""
		if len(src) > 0:
			srcText = " [%s @ %d]" % (src[0], src[1])
		result += "%08x %08x %08x %08x %08x %s%s\n" % (bp, ptrPtr(bp+4), ptrDWord(bp+8), ptrDWord(bp+12), ptrDWord(bp+16), sym, srcText)
		ip = ptrPtr(bp+4)
		bp = ptrPtr(bp)
	return result

if not is64Bit:
	# Case 1: EBP is OK, ESP (and possibly EIP) is broken
	# Resolution: Set ESP to what EBP is pointing to
	if bpOk and not spOk:
		dprintln("<u>Candidate call stack</u>:", True)
		if ipOk:
			# This represents the situation when the function was entered
			# and not at the current instruction pointer, but it means we're
			# not forced to miss a frame.
			sp = bp
			dprintln("<i>Warning: ESP represents the value at function entry time.</i>", True)
		else:
			# EBP+4 is where the return address resides
			ip = ptrPtr(bp+4)
			sp = ptrPtr(bp)
		if not resetRegisters:
			if rawBpWalk:
				dprintln(stackFromRawBpWalk(bp, sp, ip))
			else:
				dprintln(dbgCommand("kb = ebp %08x %08x" % (sp, ip)))
			dprintln('<exec cmd="r esp=%08x; r eip=%08x">Set ESP=%08x, EIP=%08x</exec>\n' % (sp, ip, sp, ip), True)
		else:
			dbgCommand("r esp = %08x" % sp)
			dbgCommand("r eip = %08x" % ip)
			dprintln(dbgCommand("kb"))

	# Case 2: EBP (and possibly EIP) is broken
	# Resolution: From ESP/stack limit, find something that looks like a [saved EBP, return address] pair
	# This is also what we do if the user passed in the "-force" switch.
	if force or not bpOk:
		if force or not spOk:
			sp = stackLimit+4
		if missingTeb:
			stackBase = sp+1024
		candidateStacks = []
		while sp < stackBase:
			sym = findSymbol(ptrPtr(sp))
			potentialBp = ptrPtr(sp-4)
			if isValid(potentialBp) and ('!' in sym or '+' in sym) and inStackLimits(potentialBp):
				if spOk and ipOk:
					potentialIp = ip
					potentialSp = sp-4
					potentialBp = sp-4
				else:
					potentialIp = ptrPtr(sp)
					potentialSp = ptrPtr(potentialBp)
				if rawBpWalk:
					kbOutput = stackFromRawBpWalk(potentialBp, sp, potentialIp)
				else:
					kbOutput = dbgCommand("kb = %08x %08x %08x" % (potentialBp, sp, potentialIp))
				kbOutputLines = kbOutput.split('\n')
				cleanKbOutput = "\n".join(kbOutputLines[1:])
				if not any(cleanKbOutput in stack for stack in candidateStacks) and "RtlUserThreadStart" in kbOutputLines[-2]:
					dprintln("<u>Candidate call stack</u>:", True)
					dprintln(kbOutput)
					dprintln('<exec cmd="r ebp=%08x; r esp=%08x; r eip=%08x">Set EBP=%08x, ESP=%08x, EIP=%08x</exec>\n' % (potentialBp, sp, potentialIp, potentialBp, sp, potentialIp), True)
					candidateStacks.append(cleanKbOutput)
					if not force and spOk:
						if resetRegisters:
							dbgCommand("r ebp = %08x" % potentialBp)
							dbgCommand("r esp = %08x" % sp)
							dbgCommand("r eip = %08x" % potentialIp)
						break
					# Else, this could be a really far-fetched guess so it would be
					# a good idea to display all candidates to the user.
			sp += 4
		dprintln("<b>Total call stacks dumped</b>: %d" % len(candidateStacks), True)

else:
	# On 64 bit, RBP is not used as a frame register.
	# Case 3: We need to find a valid return address on the stack and
	# reconstruct from there onwards with RSP immediately before that address.
	# There can be considerably more false positives here, but at least if
	# only RIP was overwritten so it's going to be the first thing we see.
	if not spOk:
		sp = stackLimit+8
	if missingTeb:
		stackBase = sp+1024
	candidateStacks = []
	while sp < stackBase:
		sym = findSymbol(ptrPtr(sp))
		if ('!' in sym or '+' in sym):
			potentialIp = ptrPtr(sp)
			potentialSp = sp+8
			kbOutput = dbgCommand("k = %016x %016x 1000" % (potentialSp, potentialIp))
			kbOutputLines = kbOutput.split('\n')
			cleanKbOutput = "\n".join(kbOutputLines[1:])
			if not any(cleanKbOutput in stack for stack in candidateStacks) and "RtlUserThreadStart" in kbOutputLines[-2]:
				dprintln("<u>Candidate call stack</u>:", True)
				dprintln(kbOutput)
				dprintln('<exec cmd="r rsp=%016x; r rip=%016x">Set RSP=%016x, RIP=%016x</exec>\n' % (potentialSp, potentialIp, potentialSp, potentialIp), True)
				candidateStacks.append(cleanKbOutput)
				if not force and spOk:
					if resetRegisters:
						dbgCommand("r rsp = %016x" % potentialSp)
						dbgCommand("r rip = %016x" % potentialIp)
					break
		sp += 8
	dprintln("<b>Total call stacks dumped</b>: %d" % len(candidateStacks), True)

if dmlWasOn:
	dbgCommand(".prefer_dml 1")
