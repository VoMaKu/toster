# Tester
This program check and compares user's programs with right answers.\
Tester include 4 directories:
* /contest: 
+ /code: where user's codes are kept.
+ /log: where tester results are kept.
+ /tests: where answers and tests of problems are kept.
* /src : where the code of tester is kept.
* /bin : where tester program is kept.
* /tmp : where binary codes of user's codes are kept.
___
## For correctly work need:
1. Create cfg files in code directory (*contest/code/user.cfg*) which contain the number of users
2. Create cfg files in tests directory (*contest/tests/problem.cfg*) which contain the number of problems
3. Create cfg file in each test (*contest/tests/A/problem.cfg*) which contain the used checker and how many examples are in it
___
## Makefile
to compile a judge and test need to do this command : **make all**
___
## Start tester!
**cd bin** \
**./judge** if you want to check all users' programs

or

**cd bin** \
**./test ../contest/code/Kozhemyak/A.c ../contest/tests/A** if you want to check local user for a specific problem
___
## contest/log
All results are stored in *contest/log*.\
Summary is in the results.log.\
Time results is in the results2.log\
Individual result is in the Kozhemyak_A.log.
___
## ENJOY
# ;D

