# 简介

本项目用于实现协作与竞争并存的社交网络影响最大化问题（Complementary and Competitive Influence Maximization, C2IC）。
输入一整个图的信息、正面与负面信息的种子节点，以及预算$k$等参数，算法获取$k$个“增强节点”使得正面信息尽可能被增强，负面信息尽可能被削弱。

信息的扩散采用独立级联模型，每个节点在某一轮接收到某种信息之后，它会在下一轮尝试向所有邻居传播该信息。共有以下4类信息在图中传播：
* 普通正面信息（记作Ca）；
* 负面信息（记作Cr）；
* 增强正面信息（记作Ca+），当某个增强节点接收到Ca之后，它会将其增强为Ca+并继续传播。Ca+比Ca具有更强的传播力；
* 中性信息（记作Cr-），当某个增强节点接收到Cr之后，它会将其削弱为Cr-并继续传播，使之抢占Cr的传播力。

在独立级联模型中，节点在某一轮接收到信息之后，之后的每一轮中它不会接收新的信息。
上述4种信息具有优先级差异，若多种信息在同一轮到达同一个节点，那么该节点只会接收优先级最高的一种信息。
图中每一条边$u \rightarrow v$具有两个权值$p_{uv} \le p_{uv}^+$，后者与前者分别表示Ca+和其余3种信息沿该条边传播的概率。
当节点$u$尝试向节点$v$传播信息时，有$p_{uv}$或$p^+_{uv}$的概率成功。若成功则$v$获得与$u$同种的信息，若失败则无事发生。

设增强节点集合为$S$，算法的目标函数为
$$H(S) = \lambda (\mathbb{E}[I_a(S) - I_a(\emptyset)]) + (1 - \lambda) (\mathbb{E}[I_r(\emptyset) + I_r(S)])$$
其中$I_a(S)$和$I_r(S)$分别表示在增强节点集合$S$的影响下，收到正面信息（包括Ca+和Ca）和负面信息（Cr）的节点个数。$\lambda \in [0,1]$为权重参数。

# 算法原理

本算法基于[[1]](https://arxiv.org/abs/1602.03111)提出的“潜在反向可达图采样”（Potentially Reverse Reachable Sketch, PRR-Sketch）。

目标函数$H(S)$的单调性与子模性取决于信息的优先级序列。24种优先级序列可以分为如下3种情况：
1. 单调、子模（对应算法PR-IMM）
2. 单调、非子模（对应算法SA-IMM）
3. 非单调、非子模（对应算法SA-RG-IMM）

对于第一种情况，算法进行$R$次反向可达图采样。
每次等概率随机选取图$G(V,E)$中的一个节点$v$作为中心，并对每条边分别进行一次采样，采样结果可能是阻塞（信息无法传播）、活跃（允许Ca+以外的信息传播）和增强（允许所有信息传播）三者之一。
然后从中心节点出发，在反图$G^T(V,E')$中（所有边方向反转）沿着活跃和增强边进行广度优先搜索，直至在某一轮遇到若干个种子节点为止。
该过程经过的所有点和边构成一个采样$g$。
随后对于采样中的每个点$s$，尝试将其设为增强节点并计算它对中心节点的信息状态产生的影响$gain(\{s\}; v)$。

基于上述反向可达图模型，目标函数等效于如下形式：
$$H(S) = \lvert V \rvert \times \mathbb{E}_{v,g} (gain(S; v)) \approx \frac{\lvert V \rvert}{R} \sum_{i=1}^{R} gain(S; v_{i},g_i)$$

基于单调子模性，上述问题可以转化为集合最大覆盖问题并用贪心算法获取近似解。
给定参数$\epsilon, \ell$，可以证明当采样次数不小于某个$R(\epsilon, \ell)$时，该算法有$1 - |V|^{\ell}$的概率返回一个近似比为$1 - 1/e - \epsilon$的解。

后两种情况采用三明治方法（Sandwich Approach），对于目标函数的某个上界和某个下界分别求解，获取两组求解结果$S_U$和$S_L$。其中：
* 上界$S_U$假定优先级为Ca+ > Cr- > Cr > Ca，然后使用上述PR-IMM算法求解；
* 下界$S_L$则基于某个单调子模的下界函数$H_L(S)$，进行若干次采样之后贪心求解。

对于第三种情况，在上述基础上下界函数$H_L(S)$的求解过程变更为随机贪心（Random Greedy）。

# 编译需求

编译器需要支持C++20标准中除`<format>`外的全部特性，包括但不限于：
* Concepts
* Ranges（`namespace std::ranges`下的全部内容，包括`<algorithm>`和`<ranges>`）
* `std::source_location`等标准库组件

详见：[Compiler Support](https://en.cppreference.com/w/cpp/compiler_support)

若编译器尚未支持`<format>`与`std::format`系列标准库组件，则需要安装并使用[fmt](https://fmt.dev)动态链接库。

# 程序使用说明

程序支持两种模式设置采样次数：
* 根据$\epsilon, \ell$等参数自动设置；
* 直接指定为一系列固定值。

若选择后一种方案，程序支持传入多个$R_1 < R_2 < \cdots < R_m$并批量获取结果，从而将实验耗时从$O(\sum_{i=1}^{m} R_i)$节约至$O(R_m)$。

在采样完成并获取增强节点集合之后，正向模拟独立级联模型中的信息传递过程，并将多次模拟的结果取均值作为最终的输出结果。
算法的最终结果等于使用该增强节点集合时的$H(S)$与不使用时的$H(\emptyset)$之差。

类似地，利用贪心算法的选取序列中贡献递减的性质，程序支持一次传入多个$k_1 < k_2 < \cdots < k_m$并批量获取结果。在获取$k_m$个增强节点后，
每次取长度为$k_i$的前缀进行上述正向模拟过程。

# 运行参数
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
* `-sample-limit-sa`: Limit of theta_sa in SA-(RG-)IMM algorithm [default: +inf]
* `-epsilon-sa`: $\epsilon_2$ in SA-(RG-)IMM algorithm [required]
* `-ell`: $\ell$ [default: 1]

## Greedy algorithm

`-greedy-test-times`: How many times to repeat per forward simulation during greedy algorithm. [default: 10000]