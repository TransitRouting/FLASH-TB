
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# FLASH-TB: Integrating Arc-Flags and Trip-Based Public Transit Routing
This repository is based on the ULTRA framework. For more information, see [ULTRA on GitHub](https://github.com/kit-algo/ULTRA). To get actual GTFS data, see [gtfs.de](https://gtfs.de/) or [transit.land](https://www.transit.land/). In addition, it is worth stopping by [here](https://www.youtube.com/watch?v=dQw4w9WgXcQ).

This repository contains the code for

* *UnLimited TRAnsfers for Multi-Modal Route Planning: An Efficient Solution* Moritz Baum, Valentin Buchhold, Jonas Sauer, Dorothea Wagner, Tobias Zündorf In: Proceedings of the 27th Annual European Symposium on Algorithms (ESA'19), Leibniz International Proceedings in Informatics, pages 14:1–14:16, 2019 pdf arXiv
* *Arc-Flags Meet Trip-Based Public Transit Routing (Arc-Flag TB)* 
Ernestine Großmann, Jonas Sauer, Christian Schulz, Patrick Steil
In: Proceedings of the 21st International Symposium on Experimental Algorithms (SEA 2023), Schloss Dagstuhl - Leibniz-Zentrum für Informatik, pages 16:1-16:18, 2023
[pdf](https://drops.dagstuhl.de/opus/volltexte/2023/18366/pdf/LIPIcs-SEA-2023-16.pdf)

If you use this repository, please cite our work using

```
@inproceedings{gromann_et_al:LIPIcs.SEA.2023.16,
	title        = {{Arc-Flags Meet Trip-Based Public Transit Routing}},
	author       = {Gro{\ss}mann, Ernestine and Sauer, Jonas and Schulz, Christian and Steil, Patrick},
	year         = 2023,
	booktitle    = {21st International Symposium on Experimental Algorithms (SEA 2023)},
	publisher    = {Schloss Dagstuhl -- Leibniz-Zentrum f{\"u}r Informatik},
	address      = {Dagstuhl, Germany},
	series       = {Leibniz International Proceedings in Informatics (LIPIcs)},
	volume       = 265,
	pages        = {16:1--16:18},
	doi          = {10.4230/LIPIcs.SEA.2023.16},
	isbn         = {978-3-95977-279-2},
	issn         = {1868-8969},
	url          = {https://drops.dagstuhl.de/opus/volltexte/2023/18366},
	editor       = {Georgiadis, Loukas},
	urn          = {urn:nbn:de:0030-drops-183664},
	annote       = {Keywords: Public transit routing, graph algorithms, algorithm engineering}
}
```

This repo also contains the code for the following publications:

* *UnLimited TRAnsfers for Multi-Modal Route Planning: An Efficient Solution*
Moritz Baum, Valentin Buchhold, Jonas Sauer, Dorothea Wagner, Tobias Zündorf
In: Proceedings of the 27th Annual European Symposium on Algorithms (ESA'19), Leibniz International Proceedings in Informatics, pages 14:1–14:16, 2019 [pdf](https://drops.dagstuhl.de/opus/volltexte/2019/11135/pdf/LIPIcs-ESA-2019-14.pdf) [arXiv](https://arxiv.org/abs/1906.04832)

* *Integrating ULTRA and Trip-Based Routing*
Jonas Sauer, Dorothea Wagner, Tobias Zündorf
In: Proceedings of the 20th Symposium on Algorithmic Approaches for Transportation Modelling, Optimization, and Systems (ATMOS'20), OpenAccess Series in Informatics, pages 4:1–4:15, 2020 [pdf](http://i11www.ira.uka.de/extra/publications/swz-iultr-20.pdf)

* *Fast Multimodal Journey Planning for Three Criteria*
Moritz Potthoff, Jonas Sauer
In: Proceedings of the 24th Workshop on Algorithm Engineering and Experiments (ALENEX'22), SIAM, pages 145–157, 2022 [pdf](https://epubs.siam.org/doi/epdf/10.1137/1.9781611977042.12) [arXiv](https://arxiv.org/abs/2110.12954)

* *Efficient Algorithms for Fully Multimodal Journey Planning*
Moritz Potthoff, Jonas Sauer
Accepted for publication at the 22nd Symposium on Algorithmic Approaches for Transportation Modelling, Optimization, and Systems (ATMOS'22)

## Usage
Before building anything, make sure to also download the submodules.

To compile all executables in release mode, run

```bash
mkdir -p cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --target All --config Release
```

Make sure you have OpenMP installed. This will create the executables ``FLASHTB``, ``Network``, ``ULTRA`` and ``TP``

 In the ``Runnables/`` directory you will find some example files on how to use the FLASH TB algorithm.

1. Start by downloading a GTFS dataset using ``downloadExample.sh``.

2. Then run ``./FLASHTB`` and run ``runScript example.script``. This will build all the necessary binaries, including trip.binary and raptor.binary, as well as the layout graph in METIS format.

3. This METIS file can be partitioned using a graph partitioner of your choice.  For example:

```./KaHIP/deploy/kaffpaE FLASH-TB/Datasets/Karlsruhe/raptor.layout.graph --k=32 --imbalance=5 --preconfiguration=social --time_limit=60 --output_filename=FLASH-TB/Datasets/Karlsruhe/raptor.partition32.txt```

4. After this step, you can call ``runScript exampleFLASHTB.script`` in ``./FLASHTB`` to build FLASHTB based on the calculated partition. This will also evaluate query performance.

## Example

```
> computeArcFlagTB ../Datasets/Karlsruhe/tripULTRA.binary ../Datasets/Karlsruhe/arctrip.binary
(...)
SplitStopEventGraph Info:
	Number of vertices: 1854595
	Number of local edges: 885771
	Number of transfer edges: 3215901
	Total number of edges: 4101672
	Number of partitions (k): 32
Computing ARCFlags with 6 threads.
Start by collecting all the departure stopevents into the approriate stop bucket!
100.00% (149ms)
Starting the computation!
100.00% (3m 16s 338ms)
Preprocessing done!
Now deleting unnecessary edges
Arc-Flag Stats:
Number of Flags set:  7625150 (10%)
Number of removed edges:  1864445 (45%)
100.00% (614ms)
Saving the compressed flags!
Done with compressed flags!
> runTransitiveArcTripBasedQueries ../Datasets/arctrip.binary 10000
(...)
Scanned trips: 389.49
Total time: 110µs
Avg. journeys: 1.52
```

Using the executable ``TP``, you can also compute Transfer Patterns given a trip binary.
```
> computeTPUsingTB ../Datasets/Karlsruhe/trip.binary ../Datasets/Karlsruhe/tp.binary
(...)
Building the Direct-Connection-Lookup Datastructure
100.00% (15ms)
Building the Stop-Lookup Datastructure
100.00% (3ms)
Computing Transfer Pattern with 6 # of threads!
100.00% (3m 32s 998ms)
Total Size: 2.20GB
Average # Nodes:  16,439.00
Average # Edges:  55,323.00
> runTPQueries ../Datasets/Karlsruhe/tp.binary 10000 false
(...)
Info about Transfer Pattern:
	Total # of vertices:  70,560,289
	Total # of edges:  237,448,327
	Max # of vertices:  44,240
	Max # of edges:  179,791
	Storage usage of all TP:  2.20GB
#### Stats ####
# Vertices in Query Graph  : 43.80
# Edges in Query Graph : 42.81
# Settled Vertices : 25.87
# Relaxed Transfer Edges : 48.67
# Added Labels into bags : 26.31
Load and build Query Graph : 9µs
Clear all Datastructures : 15µs
Initialize Source Labels : 0µs
Evaluate Query Graph : 30µs
Extract Journeys : 0µs
Total Time : 56µs
Avg. journeys  : 1.53
```
