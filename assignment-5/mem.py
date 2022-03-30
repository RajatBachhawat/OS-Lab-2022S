import math
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
mem1 = pd.read_csv('t11.csv', header=None)
time1 = pd.read_csv('t12.csv', header=None)
mem2 = pd.read_csv('t21.csv', header=None)
time2 = pd.read_csv('t22.csv', header=None)
print(time1.sum()/100)
print(time2.sum()/100)
plt.plot(time1.sum()/100, mem1.sum()/100, label='Without Garbage Collection')
plt.plot(time2.sum()/100, mem2.sum()/100, label='With Garbage Collection')
plt.xlabel('Time (ms)')
plt.ylabel('Free Memory (bytes)')
plt.xticks(range(0, math.ceil(max(time2.sum()/100))+1))
plt.legend(loc="lower left")
plt.show()
