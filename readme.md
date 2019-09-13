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


Implemented Features
* Functions
```
f = function(a, b) {
    return a * b;
};
print(f(5, 4));
```
* Local Variables
```
f = function() {
    this_is_a_local = "hello world";
    print(this_is_a_local);
};
```
* Global Variables
```
this_is_a_global = "Hmm";
f = function() {
    print(this_is_a_global);
};
```
* If Statements
* While Statements
* Lists
```
list = [];
i = 0;
while (i < 10) {
    list.append(i);
    i = i + 1;
}
```
* Short-circuiting logical operators "and" and "or"
```
while (a and b or c) {
    print("ok");
}
```

## How to build
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
