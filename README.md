# You Spin Me Round Robin

This lab assignment simulated Round Robin scheduling, a preemptive scheduling algorithm used by OSes to manage process execution time. It takees list of processes as input - each with a specified arrival time and burst time - and schedules them based on a given time quantum.

## Building

```shell
make
```

## Running

cmd for running 
```shell
./rr processes.txt 3
```

results 
```shell
Average waiting time: 7.00
Average response time: 2.75
```

## Cleaning up

```shell
make clean
```
