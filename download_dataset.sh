#!/bin/bash

wget http://www.cs.virginia.edu/~skadron/lava/Rodinia/Packages/rodinia_3.1.tar.bz2 -P evaluation
tar -xvjf evaluation/rodinia_3.1.tar.bz2 -C evaluation/ --strip-components=1 rodinia_3.1/data
rm evaluation/rodinia_3.1.tar.bz2

