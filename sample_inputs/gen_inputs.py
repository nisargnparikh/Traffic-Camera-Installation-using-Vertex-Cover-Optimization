#!/usr/bin/python3

import subprocess
import time

subprocess.Popen("> 5-10-15_graphs.txt", shell=True)
graphs = [5] * 10 + [10] * 10 + [15] * 10 
for v in graphs:
    subprocess.Popen("./graphGen {0} >> 5-10-15_graphs.txt".format(v), shell=True)


subprocess.Popen("> 20v_graph.txt", shell=True)
graphs = [20] * 1
for v in graphs:
    subprocess.Popen("./graphGen {0} >> 20v_graph.txt".format(v), shell=True)


subprocess.Popen("> 5v_to_1500v_graphs.txt", shell=True)
graphs = list(range(5,1505,10))
for v in graphs:
    subprocess.Popen("./graphGen {0} >> 5v_to_1500v_graphs.txt".format(v), shell=True)
    time.sleep(0.1)
