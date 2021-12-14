# Atomic maximum/minimum

Paper P0493 for WG21

authors: Al Grant (al.grant@arm.com), Bronek Kozicki (brok@spamcop.net), Tim Northover (tnorthover@apple.com)
audience: SG1, LEWG

# Benchmarks

* `bench1` populates fixed size queue with empty data
* `bench2` finds maximum value from a series generated by a PRNG

Benchmark results from `bench1` have typically higher standard deviation and are more susceptible to outside noise, due to contention in the Gong algorithm in CAS loop searching for the next free entry.
