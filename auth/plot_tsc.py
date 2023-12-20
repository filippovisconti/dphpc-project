import os
from pprint import pprint
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

repetitions: int = 15


def parse_csv(filename: str) -> list[np.ndarray]:

    try:
        data = pd.read_csv(filename, header=None, sep=",")
        np_arrays = [np.array(row)[:-1] for _, row in data.iterrows()]
        print("np_arrays", len(np_arrays))
        return np_arrays

    except Exception as e:
        print(e)
        exit(1)


def grab_input_sizes():
    with open("input_sizes.txt", 'r') as file:
        input_sizes = file.read().splitlines()

    # delete lines startig with #
    raw_input_sizes = [x for x in input_sizes if not x.startswith("#")]

    # convert to bytes
    input_sizes = []
    for size_str in raw_input_sizes:
        size = int(size_str[:-2])  # Extract the numeric part
        unit = size_str[-2:]  # Extract the unit (KB, MB, GB)

        if unit == "KB":
            size *= 1024
        elif unit == "MB":
            size *= 1024 * 1024
        elif unit == "GB":
            size *= 1024 * 1024 * 1024

        input_sizes.append(size)
    return input_sizes, raw_input_sizes


def plot_data(v: str = 'f'):

    input_sizes, raw_input_sizes = grab_input_sizes()
    print(raw_input_sizes)
    # iterate over all files in the directory output_data
    to_plot = []
    for filename in sorted(os.listdir("output_data"), reverse=True):
        if not filename.endswith(".csv"):
            continue
        func_name = filename.split(".")[0][:-3]
        num_threads = (filename.split(".")[0].split("_")[-1])
        if func_name == f'blake_{v}':
            to_plot.append((func_name, num_threads))
    medians = []
    devs = []
    for fn_name, n_thr in to_plot:
        data = parse_csv(f"output_data/{fn_name}_{n_thr.zfill(2)}.csv")
        print(f"Preparing data for {fn_name} , {n_thr} threads")
        tmp_lst = []
        tmp_devs = []
        for i, row in enumerate(data):
            arr = row
            arr = input_sizes[i] / arr
            median = np.median(arr)
            std_dev = np.std(arr)
            tmp_lst.append(median)
            tmp_devs.append(std_dev)
        devs.append(tmp_devs)
        medians.append(tmp_lst)
    print(len(medians))
    print(len(devs))
    plt.style.use("ggplot")
    plt.figure(figsize=(13, 12))
    fig, ax = plt.subplots(dpi=600)
    skip = 9
    for k, (func_name, n_th) in enumerate(to_plot):
        print("Output", func_name, n_th)
        print(len(medians[k]), len(devs[k]), len(input_sizes))
        ax.errorbar(
            x=input_sizes[:len(medians[k])][skip:],
            y=medians[k][skip:], yerr=devs[k][skip:],
            label=f"{func_name} {n_th}t",
            linestyle='-',
            linewidth=0.7,
            # barsabove=True,
            capsize=2, fmt='o', markersize=3)

    ax.set(xlabel='Input Sizes', ylabel='Throughput (B/c)',
           title='Benchmark Results - Full Tree Version')
    plt.legend(facecolor="white")
    plt.grid(axis="x", color="#E5E5E5")
    plt.xscale("log")
    plt.minorticks_off()
    ax.set_xticks(ticks=input_sizes[skip:],
                  labels=raw_input_sizes[skip:], minor=False)
    plt.xticks(rotation=90)

    ax.legend()
    if not os.path.exists("figures"):
        os.makedirs("figures")
    txt = "Higher is better.  AMD EPYC 7501 @ 2GHz, 64 cores, 512GB RAM. GCC compiler, -O3 optimization level."
    plt.figtext(0.5, -0.1, txt, wrap=True, horizontalalignment='center', fontsize=8)

    filename: str = "figures/plot_cumulative_f.png"

    plt.savefig(filename, bbox_inches="tight", pad_inches=0.1)


if __name__ == "__main__":
    plot_data(v='f')
    plot_data(v='d')
