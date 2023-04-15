import os
import re
import os
import sys

re_formula = re.compile(r'^(FOUND throughput\s)(\d*\s)(per 1s)$')

throughput=0
dir_name=sys.argv[1]
load=int(sys.argv[2])
for root, dirs, files in os.walk(dir_name, topdown=False):
    for name in files:
        print(os.path.join(root, name))
        found = False
        for line in open(os.path.join(root, name)):
            match_str = re.match(re_formula, line)
            if match_str != None:
                found = True
                print(match_str.group(2))
                throughput = throughput+ int(match_str.group(2))
        if not found:
            print("not saturated, use ", load)
            throughput = throughput + load

print("total throughput: ", throughput)
