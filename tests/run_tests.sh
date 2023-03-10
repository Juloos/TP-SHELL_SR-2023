#!/bin/bash


RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'


# On compare le resultat des commandes entre notre shell et sh
for test in tests/*.txt
do
    ./sdriver.pl -t $test -s /bin/sh | sort > tests/default
    ./sdriver.pl -t $test -s ./shell | sort > tests/output
    if diff tests/default tests/output > tests/tmp;
    then
        echo -e ${GREEN}passed $test ${NOCOLOR}
    else
        # On crée un fichier de log si le test échoue
        mv tests/tmp $test.log
        echo -e ${RED}failed $test ${NOCOLOR}
    fi
done

# On verifie que valgrind ne renvoie pas d'erreur sur nos tests
for valgrind_test in tests/*.txt
do
    ./sdriver.pl -t $valgrind_test -s /usr/bin/valgrind  -a "-q --leak-check=full --show-leak-kinds=all ./shell" 2> tests/output 1> /dev/null
    if [ -s tests/output ]; then
        echo -e ${RED}failed valgrind $valgrind_test ${NOCOLOR}
        # On crée un fichier de log si le test échoue
        mv tests/output $valgrind_test.log
    else
        echo -e ${GREEN}passed valgrind $valgrind_test ${NOCOLOR}
    fi
done

rm -f tests/default tests/output tests/tmp