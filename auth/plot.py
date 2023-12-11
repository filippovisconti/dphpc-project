import json
import matplotlib.pyplot as plt

def load_data_from_json(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    return data

def plot_data(data):
    input_sizes = ["8KB", "32KB", "64KB",  "512KB", "1MB", "2MB", "4MB", "16MB",
                   "32MB", "64MB", "128MB", "256MB", "512MB", "1GB"]

    plt.style.use("ggplot")
    fig, ax = plt.subplots()

    for k,func_name in enumerate([ "sha256","ref_blake3", "my_blake3"]):
        print(k, func_name)
        cycles = [int(data["threads"]["0"]["regions"][str(i)]["PAPI_TOT_CYC"])/2250 for i in range(k,15*3*(len(input_sizes)),15*3) if i < 630]
        print(cycles)
        ax.plot(input_sizes, cycles, label=func_name)

    ax.set(xlabel='Input Sizes', ylabel='Cycles',
           title='Benchmark Results')
    ax.legend()
    plt.legend(facecolor="white")
    plt.grid(axis="x", color="#E5E5E5")
    plt.yscale("log")
    plt.show()

if __name__ == "__main__":
    json_file = "./papi_output/papi_hl_output-20231124-004237/rank_000000.json"  # Replace with the actual path to your JSON file
    data = load_data_from_json(json_file)
    plot_data(data)
