# DETER
DETER is a system that can replay the execution of TCP connections. See our [NSDI 19 paper](https://www.usenix.org/conference/nsdi19/presentation/li-yuliang).

It requires a few modifications to the Linux kernel to support. We made modifications to Linux 4.4.98 [here](https://github.com/harvard-cns/deter_kernel_4.4.98).
These modifications are basically hook functions that we added to record (during runtime) and inject (during replay) values.

## Introduction
### Record
For record, DETER has a kernel module and a user program. 

The kernel module implements the hook functions that record values. The recorded values are stored in kernel memory shared with the user space.

The user program reads recorded values from the shared memory. When a connection finishes, the user program dump the data to disk.

### Replay
For replay, DETER also has a kernel module and a user program.

The kernel module implements the hook functions that return values, which are get from the user program.

The user program simply reads values from local disk, and feed them to the kernel module via the shared memory.

## Usage
First compile and install the kernel.

Then `make` in `kmod/` and in `user/`.

In `user/`, use `run_record.sh` to run a recorder. The recorder currently are configured to record connections with port number 50010, 60000~60003. Use `stop_record.sh` to stop the recorder.

The data are stored under `user/`, with file named `<srcip(hex)>:srcport-<dstip(hex)>:dstport`.

To replay, use `run_replay.sh`. Use `stop_replay.sh` to stop the replayer.

## Questions?
You may contact <yuliangli@g.harvard.edu> for further questions.
