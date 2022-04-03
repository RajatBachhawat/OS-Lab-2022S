import math
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import numpy as np
mem1 = pd.read_csv('t11.csv', header=None)
time1 = pd.read_csv('t12.csv', header=None)
mem2 = pd.read_csv('t21.csv', header=None)
time2 = pd.read_csv('t22.csv', header=None)
mem3 = pd.read_csv('t31.csv', header=None)
time3 = pd.read_csv('t32.csv', header=None)
print(time1.sum()/100)
print(time2.sum()/100)
print(time3.sum()/100)
fig, ax = plt.subplots()
ax.xaxis.set_major_formatter(FormatStrFormatter('%0.1f'))
plt.plot(time1.sum()/100, mem1.sum()/100, label='Without Garbage Collection')
plt.plot(time2.sum()/100, mem2.sum()/100, label='With Garbage Collection')
plt.plot(time3.sum()/100, mem3.sum()/100, label='With Garbage Collection + Stack Popping by User')
plt.xlabel('Time (ms)')
plt.ylabel('Free Memory (bytes)')
plt.xticks(np.arange(0, max(max(time2.sum()/100), max(time3.sum()/100)), max(time2.sum()/100)/25))
plt.legend(loc="lower left")
plt.show()
