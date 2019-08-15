Currently this is a work in progress, it's nowhere near completion

The project depends on my other project hasht.
the directory structure is supposed to look like:

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

```
git clone https://github.com/nilput/knit
cd knit
git clone https://github.com/nilput/hasht
make
```
then you can try 
`
./test -i
`
