# Progress

* PRR-Sketch sampling tested OK.
* PR-IMM tested OK.

# To-do list

* SA-IMM for non-submodular cases
* SA-RG-IMM for non-monotonic and non-submodular cases

# Requirements

* For MSVC: Use Visual Studio 2022 and `/std:c++latest` compiler flag
* For GCC: 
  * Version >= GCC 11, and use `-std=c++20` compiler flag
  * Install C++ lib via `sudo apt install libfmt-dev` and use `-lfmt` compiler flag 

# Usage

* `-h --help`: shows help message and exits [default: false]
* `-v --version`: prints version information and exits [default: false]
* `-graph`: Path of the graph file [required]
* `-seedset`: Path of the seed set file [required]
* `-algo`: The algorithm to use: `Auto`, `PR-IMM`, `SA-IMM` or `SA-RG-IMM` [default: `Auto`]
* `-k`: Number of boosted nodes [default: 10]
* `-sample-limit`: Maximum number of PRR-sketch samples [default: +inf]
* `-theta-sa-limit`: Limit of theta_sa in SA-IMM algorithm [default: +inf]
* `-test-times`: How many times to check the solution by forward simulation [default: 10000]
* `-log-per-percentage`: Frequency for debug logging [default: 5]
* `-epsilon`, `-epsilon1` or `-epsilon-pr`: Epsilon1 in PR-IMM algorithm [default: 0.1]
* `-epsilon2` or `-epsilon-sa`: Epsilon2 in SA-IMM algorithm [default: the same as `-epsilon`]
* `-gain-threshold-sa`: Minimum average gain for each node in SA-IMM algorithm [default: 0]
* `-lambda`: Lambda [default: 0.5]
* `-ell`: Ell [default: 1]
* `-priority`: Priority of the 4 states. Input from highest to lowest. 
  * e.g. `-priority Ca+ Cr- Cr Ca` refers to Ca+ > Cr- > Cr > Ca 
  * [default: `Ca+ Cr- Cr Ca`]