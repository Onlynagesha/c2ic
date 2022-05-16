# Requirements

* For MSVC: Use Visual Studio 2022 and `/std:c++latest` compiler flag
* For GCC: 
  * Version >= GCC 11, and use `-std=c++20` compiler flag
  * Install C++ lib via `sudo apt install libfmt-dev` and use `-lfmt` compiler flag 

# Usage
## Common arguments
* `-h --help`: shows help message and exits [default: false]
* `-v --version`: prints version information and exits [default: false]
* `-graph-path`: Path of the graph file [required]
* `-seed-set-path`: Path of the seed set file [required]
* `-algo`: The algorithm to use: `Auto`, `PR-IMM`, `SA-IMM`, `SA-RG-IMM`, `Greedy`, `MaxDegree` or `PageRank` [default: `Auto`]
* `-k`: Number of boosted nodes [required]
* `-priority`: Priority of the 4 states. Input from highest to lowest, tokens separated with spaces, commas or `>`.
e.g. `-priority Ca+ Cr- Cr Ca` refers to Ca+ > Cr- > Cr > Ca
[required]
* `-lambda`: Weight parameter $\lambda$ of objective function [default: 0.5]
* `-log-per-percentage`: Frequency for progress logging [default: 5]
* `-test-times`: How many times to check the solution by forward simulation [default: 10000]

`-k` can be either a single positive integer (e.g. `10`), 
or a list of positive integers separated by spaces, commas or semicolons.
e.g. (`10,20,30` or `10; 20; 30`). The maximum value of k is used in the algorithm.
During simulation, however, all the k's are attempted 
(since boosted nodes are sorted in descending order by their influence, 
the result of boosted nodes with $k = k_1$ is simply the prefix of that with $k = k_2 > k_1$). 

## PR-IMM algorithm

PR-IMM algorithm supports two modes during sampling. 

If the argument `-n-samples` is provided,
static sample set size is used where the number of PRR-sketch samples is a (series of) fixed value(s).

Otherwise, the sample set size $\delta$ is determined dynamically by $\epsilon$, $\ell$ 
and a limit for early stop.

### Static sample set size
* `-n-samples`: Fixed number of samples.

`n-samples` can be either a single positive integer, or a list of positive integers
separated by spaces, commas or semicolons. For two sample set sizes $R_1$ and $R_2$ such that $R_1 < R_2$,
the latter is performed as appending $R_2 - R_1$ more samples on the former.

### Dynamic sample set size
* `-sample-limit`: Maximum number of PRR-sketch samples [default: +inf]
* `-epsilon`: $\epsilon$ in PR-IMM algorithm [required]
* `-ell`: $\ell$ [default: 1]

## SA-IMM and SA-RG-IMM algorithm

SA-(RG-)IMM algorithm is performed as two parts, both of whose arguments shall be provided separately:
1. Upper bound: assumes with a monotonic & submodular priority and performs PR-IMM algorithm;
2. Lower bound (denoted as SA-(RG-)IMM-LB).

SA-(RG-)IMM-LB supports two modes during sampling:

If the argument `-n-samples-sa` is provided,
sample set size **per center node (i.e. total number of samples = number of center nodes $\times$ sample size per center node)**.
is given as a (series of) fixed value(s).

Otherwise, sample set size per center $\delta$ is determined by $\epsilon_2$, $\ell$ and a limit.

### Common arguments
* `-sample-dist-limit-sa`: Distance threshold from center nodes to any of the seeds. Nodes with higher distance are filtered out. [default: +inf]

### Static sample set size
* `-n-samples-sa`: Fixed number of samples for each center node.

`-n-samples-sa` can be either a positive integer or a list of positive integers. See `-n-samples` in PR-IMM algorithm for details.

### Dynamic sample set size
* `-epsilon-sa`: $\epsilon_2$ in SA-(RG-)IMM algorithm [required]
* `-ell`: $\ell$ [default: 1]
* `-sample-limit-sa`: Limit of theta_sa in SA-(RG-)IMM algorithm [default: +inf]

## Greedy algorithm

`-greedy-test-times`: How many times to repeat per forward simulation during greedy algorithm. [default: 10000]