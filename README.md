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
**./bin/judge** if you want to check all users' programs

or

**./bin/test contest/code/Kozhemyak contest/tests/A** if you want to check local user for a specific problem
___
## contest/log
All results are stored in *contest/log*.\
Total result is in the results.log.\
Separate result is in the Kozhemyak_A.log.
___
## ENJOY
# ;D

