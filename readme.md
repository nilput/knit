# Knit

An embeddable scripting language

Features:
 - No global state
 - Has a C API to call C functions
 - Can be included as a single header
 - Has a virtual machine, and a compiler that compiles code to vm instructions
 - No dependencies
 - Doesn't exit()


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
* Globals
```
this_is_a_global = "Hmm";
f = function() {
    print(this_is_a_global);
};
```
* If Statements
```
if (num % 2 == 0) {
    print('Even!')
}
else {
    print('Odd!')
}
```
* While Statements
```
    condition = true
    while (condition) {
        print('infinite loop!')
    }
```
* Lists
```
list = [];
i = 0;
while (i < 10) {
    list.append(i);
    i = i + 1;
}
for (i=0; i<len(list); i=i+1) {
    print(list[i])
}
```
* Other stuff
```
if (a and b or c) {
    print("ok");
}
```

## How to build
```
git clone https://github.com/nilput/knit --recursive
make
```
then you can run it interactively by: 
`
./knit
`

or run a file like:
`
./knit examples/sum.kn
`
