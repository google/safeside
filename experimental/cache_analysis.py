"""Plot measurement data.

Run instructions:

  python3 -m venv env
  source env/activate
  pip install -r experimental/requirements.lock
  python experimental/cache_analysis.py cache_size_results.csv
"""


from matplotlib.pyplot import figure
import matplotlib.pyplot as plt
import io
import numpy as np
import pandas as pd
from statistics import mean, median
import collections
import csv
import seaborn as sns
import os
import sys


def draw_single_run_plot(data, name):
  """ This functions draws and saves two types of plots for cases where each x value has only one y value.

  Here it is used to draw plots for stats (ie mean and median of all runs).
  It can be used for drawing each run individually.
  Args:
    data:
      A dictionary from x values to their corresponding y value
    name:
      The file name that is used for saving the plot 
  """
  figure(num=None, figsize=(10, 8), dpi=80, facecolor='w', edgecolor='k')

  x = list(data.keys())
  y = list(data.values())

  plt.subplot(211)
  plt.plot(x,y)
  plt.subplot(212)
  plt.scatter(x,y)
  plt.savefig(name)
  plt.cla()


def draw_boxplot(data, sizes, name):

  """ This functions draws and saves boxplot for all data points.

  Args:
    data: 
      A list of lists, where each list contains the timing values for each size under analysis
      lists are in the asecending order of the sizes values (e.g. timing values for the smallest size corresponds to data[0])  
    sizes: 
      The list of sizes under analysis that is used as the values of x axis
    name: 
      The file name that is used for saving the plot 
  """
  fig = plt.figure(figsize=(30, 10))
  ax = fig.add_subplot(111)
  boxes = []
  for l in data:
    for i, time in enumerate(l):
      boxes.append([sizes[i%len(l)], time])

  dd = pd.DataFrame(boxes, columns = ["size (bytes)", "time (cycles)"])
  sns.set_theme(style="whitegrid")

  ax = sns.boxplot(data = dd, showfliers=False, x = 'size (bytes)', y = 'time (cycles)')
  ax.grid(False)
  fig = ax.get_figure()
  fig.savefig(name)


def draw_all_datapoints(data, sizes, name):
  """ This functions draw and saves all data points as individual plots in a single figure in log scale

  Args:
    data:
      A list of lists, where each list contains the timing values for each size under analysis
        lists are in the asecending order of the sizes values (e.g. timing values for the smallest size corresponds to data[0])  
    sizes:
      The list of sizes under analysis that is used as the values of x axis
    name:
      The file name that is used for saving the plot 
  """
  fig = plt.figure(figsize=(30, 10))
  ax = fig.add_subplot(111)
  for l in data:
    ax.scatter(sizes,l)
  ax.set_yscale('log')
  ax.set_xscale('log', base = 2)
  plt.xlabel("size (bytes)")
  plt.ylabel("time (cycles)")
  plt.title(" all data points")
  plt.grid(False)
  plt.savefig(name)
  plt.cla()


def main():
  if len(sys.argv) != 2:
    sys.exit(f"Expected 1 argument, got {len(sys.argv) - 1}")

  # read csv file to extract data
  df = pd.read_csv(sys.argv[1], header=None)


  count_row = df.shape[0]  # gives number of row count
  sizes = df[0].unique()  # sizes used for the analysis
  iterations = int(count_row/len(sizes))  # total experiments' repetitions
  print(f"In total the experiment was repeated {iterations} times")
  print(f"In total {len(sizes)} different values were tested")

  # extract all runs seperately
  all = []
  pre = 0
  for i in range(1, count_row):
      if (i+1) % len(sizes) == 0:
          l = df.iloc[pre: i+1]
          pre = i+1
          all.append(l)

  # extract each size results

  all_data = []
  for i in range(iterations):
      l = list(all[i][1])
      all_data.append(list(l))

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

if __name__ == '__main__': 
  main()  
