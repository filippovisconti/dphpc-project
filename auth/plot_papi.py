import json
import os
import numpy as np
import matplotlib.pyplot as plt

repetitions: int = 15


def load_data_from_json(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    return data


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


def plot_data(data, single_thread=True, num_threads=1):

    input_sizes, raw_input_sizes = grab_input_sizes()

    func_names = ["blake3_f", "blake3_d", "ref_blake3", "sha256"]
    if not single_thread:
        func_names = ["blake3_f", "blake3_d"]

    total_runs = len(data["threads"]["0"]["regions"]
                     ) // repetitions // len(func_names)
    print("total runs per function", total_runs)

    if ((total_runs * len(func_names) * repetitions) != len(data["threads"]["0"]["regions"])):
        raise ValueError("Invalid number of runs")

    medians = []
    devs = []
    for k, func_name in enumerate(func_names):
        print(f"Preparing data for {func_name}")
        tmp_lst = []
        cycles = np.array(
            [int(data["threads"]["0"]["regions"][str(i)]["PAPI_TOT_CYC"])
             for i in range(total_runs*repetitions*k, total_runs*repetitions*(k+1))])
        for i in range(len(cycles)//repetitions):
            arr = np.array(cycles[repetitions*i:repetitions*(i+1)])
            arr = input_sizes[i] / arr
            median = np.median(arr)
            std_dev = np.std(arr)
            tmp_lst.append(median)
            devs.append(std_dev)
        medians.append(tmp_lst)

    plt.style.use("ggplot")
    plt.figure(figsize=(13, 12))
    _, ax = plt.subplots(dpi=600)
    for k, func_name in enumerate(func_names):
        print("Output", func_name)
        ax.errorbar(
            x=input_sizes, y=medians[k], yerr=devs[k], label=func_name,
            linestyle='-', barsabove=True, capsize=5, fmt='o', markersize=5)

    ax.set(xlabel='Input Sizes', ylabel='Throughput (B/c)',
           title='Benchmark Results')
    plt.legend(facecolor="white")
    plt.grid(axis="x", color="#E5E5E5")
    plt.xscale("log")
    plt.minorticks_off()
    ax.set_xticks(ticks=input_sizes, labels=raw_input_sizes, minor=False)
    plt.xticks(rotation=90)

    ax.legend()
    if not os.path.exists("figures"):
        os.makedirs("figures")

    filename: str = f"figures/plot{'_multi' if not single_thread else ''}_{num_threads}c.png"
    plt.savefig(filename, bbox_inches="tight", pad_inches=0.1)


if __name__ == "__main__":
    json_file = "./papi_output/papi_single.json"
    data = load_data_from_json(json_file)
    plot_data(data)

    json_file = "./papi_output/papi_multi_4c.json"
    data = load_data_from_json(json_file)
    plot_data(data, single_thread=False, num_threads=4)
