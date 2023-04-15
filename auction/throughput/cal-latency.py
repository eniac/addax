import os
import re
import os
import sys

re_formula = re.compile(r'^(\d+ handle time:\s)(\d*\.\d*)$')

throughput=0
dir_name=sys.argv[1]
time_list=[]
for root, dirs, files in os.walk(dir_name, topdown=False):
    for name in files:
        print(os.path.join(root, name))
        first=False
        for line in open(os.path.join(root, name)):
            match_str = re.match(re_formula, line)
            if match_str != None:
                if not first:
                    first=True
                    continue
                # print(match_str.group(2))
                time_list.append(float(match_str.group(2)))

time_list.sort()
print("median latency: ", time_list[int(len(time_list)/2)])
print("99% latency: ", time_list[int(len(time_list)*0.99-1)])
