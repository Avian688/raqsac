#!/usr/bin/env python

import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import random
   
def getResults(file):
    results = pd.read_csv(file, header=None,  names=["Goodput"])
    results = results.sort_values("Goodput")
    results = results.reset_index(drop=True)
    print(results)
    return results;

if __name__ == "__main__":
    results = []
    for arg in sys.argv[1:]:
        results.append(getResults(arg))
    for index, result in enumerate(results):
        plt.figure(figsize=(10,7))
        plt.xticks(fontsize=20)
        plt.yticks(fontsize=20)
        result.index = np.arange(1, len(result) + 1)
        rgb = (random.random(), random.random(), random.random())
        result["Goodput"] = result["Goodput"].apply(lambda x: x/1000000000)
        plt.plot(list(result.index.values),result["Goodput"], c=rgb, drawstyle='steps-post', linewidth=2)
        axes = plt.gca()
        axes.set_xlim([0, result["Goodput"].idxmax()+5])
        axes.set_ylim([0,1])
        plt.xlabel('Rank of Transport Session', fontsize=28)
        plt.ylabel('Goodput (Gbps)', fontsize=28)
        plt.title("Goodput", fontsize=35)
        #plt.xticks((np.arange(0, result["Goodput"].idxmax(), step=250)))
        plt.legend(loc="lower right")
        plt.savefig('Goodput.png')
    

