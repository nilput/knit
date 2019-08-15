Currently this is a work in progress, it's nowhere near completion

The directory structure assumes there is a copy of hasht
get it from github.com/nilput/hasht
it's supposed to look like:
    knit->
        hasht/
            src
            scripts
            ...
        src/
            test.c
            knit.h
            ...
        Makefile
        ...

`
git clone https://github.com/knit
cd knit
git clone https://github.com/hasht
make
`
then you can try 
`
./test -i
`
