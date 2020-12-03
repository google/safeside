from matplotlib.pyplot import figure
import matplotlib.pyplot as plt
import io
import numpy as np
import pandas as pd
from statistics import mean, median
import collections
import csv
import seaborn as sns

""" This functions draws two types of plots for cases where each x value has only one y value.

Here it is used to draw plots for stats (ie mean and median of all runs).
It can be used for drawing each run individually.
"""
def draw_single_run_plot(data, name):
  figure(num=None, figsize=(10, 8), dpi=80, facecolor='w', edgecolor='k')

  x = list(data.keys())
  y = list(data.values())

  plt.subplot(211)
  plt.plot(x,y)
  plt.subplot(212)
  plt.scatter(x,y)
  plt.savefig(name)


""" This functions draws boxplot for all data points.
"""
def draw_boxplot(data, x, name):
  sns.set(rc={'figure.figsize':(40,10)})
  boxes = []
  for l in data:
    c = 0
    for i in l:
      if c % 2 == 0:
        boxes.append([x[c], i])
      c += 1
  dd = pd.DataFrame(boxes, columns = ["size (bytes)", "time (cycles)"])
  sns.set_theme(style="whitegrid")

  ax = sns.boxplot(data = dd, showfliers=False, x = 'size (bytes)', y = 'time (cycles)')
  ax.grid(False)
  fig = ax.get_figure()
  fig.savefig(name)

""" This functions draw all data points in a plot in log scale
"""
def draw_all_datapoints(data, sizes, name):
  fig = plt.figure(figsize=(30, 10))
  ax = fig.add_subplot(111)
  for l in data:
    ax.scatter(sizes,l)
  # start, end = ax1.get_xlim()
  # ax1.xaxis.set_ticks(np.arange(0, end))
  # start, end = ax1.get_ylim()
  # ax1.yaxis.set_ticks(np.arange(0, end))
  ax.set_yscale('log')
  ax.set_xscale('log', base = 2)
  plt.xlabel("size (bytes)")
  plt.ylabel("time (cycles)")
  plt.title(" all data points")
  plt.grid(False)
  plt.savefig(name)



file_name = 'cache_line_size_results.csv'  # name of result file
file_path = '/home/negara/safeside/build/experimental/'  # path to the result file

# read csv file to extract data
df = None
try:
    from google.colab import files
    # colab
    uploaded = files.upload()
    df = pd.read_csv(io.StringIO(
        uploaded[file_name].decode('utf-8')), header=None)
except ImportError:
    # not colab
    import os
    df = pd.read_csv(os.path.join(file_path, file_name), header=None)


count_row = df.shape[0]  # gives number of row count
count_col = df.shape[1]  # gives number of col count
sizes = df[0].unique()  # sizes used for the analysis
iterations = int(count_row/len(sizes))  # total experiments' repetitions
print("In total the experiment was repeated ", iterations, " times")
print("In total ", len(sizes), " different values were tested")

# extract all runs seperately
all = []
pre = 0
for i in range(1, count_row):
    if (i+1) % len(sizes) == 0:
        l = df.iloc[pre: i+1]
        pre = i+1
        all.append(l)

# extract each size results
data_per_size = {}
all_data = []
for i in range(0, iterations):
    l = list(all[i][1])
    l_size = len(l)
    all_data.append([x for x in l])

    # extracting timings for each size
    for j in range(0, len(l)):
        if j not in data_per_size:
            data_per_size[j] = []
        t = data_per_size[j]
        t.append(l[j])
        data_per_size[j] = t

print("Extracted the results for each size under analysis")

#computes the median and mean of iterations
means = {}
medians = {}

grouped = df.groupby(df[0])

for cache_size in sizes:
  times = grouped.get_group(cache_size)
  means[cache_size] = mean(times[1])
  medians[cache_size] = median(times[1])

draw_single_run_plot(medians, "medians.pdf")
draw_single_run_plot(means, "means.pdf")


draw_boxplot(all_data, sizes, "boxplot.pdf")

draw_all_datapoints(all_data, sizes, "all_runs.pdf")