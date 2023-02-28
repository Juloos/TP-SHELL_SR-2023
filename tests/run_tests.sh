#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

for derterministic_test in tests/*.det.txt
do
    sh $derterministic_test | sort > default
    ./shell $derterministic_test | sort > output
    if cmp -s default output;
    then
        echo -e ${GREEN} test $derterministic_test passed ${NOCOLOR}
    else
        echo -e ${RED} test $derterministic_test failed ${NOCOLOR}
    fi
done

for valgrind_test in tests/*.txt
do
    valgrind -q --leak-check=full --show-leak-kinds=all ./shell $valgrind_test 2> output 1> /dev/null
    if [ -s output ]; then
        echo -e ${RED} valgrind $valgrind_test failed ${NOCOLOR}
        mv output $valgrind_test.log
    else
        echo -e ${GREEN} valgrind $valgrind_test passed ${NOCOLOR}
    fi
done

rm -f default output