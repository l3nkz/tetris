#!/usr/bin/env python3

import sys,os
from collections import defaultdict
from xml.dom import minidom


def parseMapping(path):
	themap = {}
	mappings = [m for m in os.scandir(path) if m.name.endswith(".mapping")]
	if (len(mappings) == 0):
		print("No valid mapping: "+path+" @"+str(len(mappings)),file=sys.stderr)
		return
	assert(len(mappings) == 1)

	doc = minidom.parse(mappings[0].path)
	schedulers = doc.getElementsByTagName("SingleSchedulerDesc")
	for i in schedulers:
		for p in i.getElementsByTagName("Process"):
			themap["t_"+p.attributes["Name"].value] = i.attributes["Name"].value[22:27]

	ov = [m for m in os.scandir(path) if m.name.endswith(".outputOverview")]
	if (len(ov) == 0):
		print("No valid overview: "+path+" @"+str(len(mappings)),file=sys.stderr)
		return
	assert(len(ov) == 1)

	with open(ov[0].path) as f:
		for l in f.readlines():
			(k,v) = l.split("=")
			themap[k] = v.rstrip()

	return themap

if len(sys.argv) < 2:
	print("Expect path as argument!")
	sys.exit(-1)

if not os.path.isdir(sys.argv[1]):
	print("Could not find dir: "+sys.argv[1])
	sys.exit(-1)

if not os.path.isdir(sys.argv[1]+"/mappings/"):
	print("Directory contains no mappings!")
	sys.exit(-1)

completeMap = defaultdict(list)
for de in [e for e in os.scandir(sys.argv[1]+"/mappings/") if e.is_dir()]:
	mappings = parseMapping(sys.argv[1]+"/mappings/"+de.name)
	if not mappings: continue
	completeMap["mapping"].append(de.name)
	for i in sorted(mappings.keys()):
		completeMap[i].append(mappings[i])

if any(len(completeMap["mapping"]) == len(t) for t in mappings.values()):
	print("Non balanced!")
	sys.exit(-2)

print("mapping",end="")
for i in sorted([m for m in completeMap.keys() if m != "mapping"]):
	print(","+i,end="")
print("")
for i in range(len(completeMap["mapping"])):
	print(completeMap["mapping"][i],end="")
	for k in sorted([m for m in completeMap.keys() if m != "mapping"]):
		print(","+completeMap[k][i],end="")
	print("")
