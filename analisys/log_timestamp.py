#!/usr/bin/env python3
# Author: pabloheralm@gmail.com
#         pablololo12

import sys
import os
import argparse
import csv

def parse_arguments():
    parser = argparse.ArgumentParser(description="Transforms date time into boot time")
    parser.add_argument('-f', help='Log file to transform', type=str,
            required=True)
    parser.add_argument('-t', help='File with relation epoch timestamp', type=str,
            required = True)
    args = parser.parse_args()
    return args

def get_data(file_n):
    data = []
    with open(file_n, 'r') as csvfile:
        data_reader = csv.reader(csvfile, delimiter=',')
        next(data_reader)
        for row in data_reader:
            data.append([f for f in row])
    return data

def hour_to_sec(hour):
    h = hour.split(":")
    return int(h[0])*3600+int(h[1])*60+float(h[2])

def transform_time_sec(times):
    secs = []
    #first = hour_to_sec(times[0])
    for d in times:
        secs.append(hour_to_sec(d))
    return secs

def main():
    args = parse_arguments()
    log = get_data(args.f)
    tim = get_data(args.t)
    log.sort()
    aux = [f[1] for f in tim]
    aux = transform_time_sec(aux)
    for i in range(len(aux)):
        tim[i].append(aux[i])

    aux2 = [f[0] for f in log]
    aux2 = transform_time_sec(aux2)
    for i in range(len(aux2)):
        log[i].append(aux2[i])

    deltas = []
    print(log)
    print(tim)
    for d in log:
        t = 0
        for i in range(len(tim)):
            if tim[i][2] > d[3]:
                t = i-1
                break
        a = d[3]-tim[t][2]
        deltas.append(a+float(tim[t][0]))
    for i in range(len(log)):
        log[i].append(deltas[i])
    print("Workload,Type,Timestamp")
    for d in log:
        print(d[1]+","+d[2]+","+str(d[4]))


if __name__ == '__main__':
    main()
