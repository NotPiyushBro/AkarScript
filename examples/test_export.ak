@export let x = 42
print("x = " + to_string(x))
x = 100
print("x after write = " + to_string(x))

@export let name = "hello"
print("name = " + name)
name = name + " world"
print("name after concat = " + name)
