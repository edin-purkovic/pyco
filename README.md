# pyco
Small embedable language for tools and game engines.

WORK IN PROGRESS

Curently building the AST parser, final syntax will be inspired by Jai and Odin. 
It will use reference counting and no plans for a garbage collector at the moment.

Compiler and the VM will be separate .c files that can be used individually.

Syntax example
```
point :: struct {
    x int32
    y int32
}

do_something :: function(a) bool {
    if a < 1 {
        return false
    }

    grid := [10][10]uint8

    for i := 0; i < 10; i++ {
        for j := 0; j < 10; j++ {
            grid[i][j] = 0
        }
    }

    z := 1

    return true
}

```


