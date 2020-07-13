Knit

An embeddable scripting language
features:
    - No global state
    - C API
    - Can be included as a single header
    - Has a virtual machine, and a compiler that compiles code to vm instructions
    - No dependencies\*

\*The project depends on my other project hasht for hashtables (also header only!)

Documentation:

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
git clone https://github.com/nilput/knit
./scripts/getdependencies.sh
make
```
then you can run it by: 
`
./knit
`
